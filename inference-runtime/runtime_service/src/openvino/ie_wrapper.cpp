// Copyright (C) 2020 Intel Corporation
//

#include <fcntl.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include <openssl/md5.h>

#include <ie_iextension.h>
#include "ie_wrapper.hpp"
#include "decrypt.h"
#include <gna/gna_config.hpp>

using namespace InferenceEngine;

namespace InferenceEngine {

std::map<std::string, InferenceEngine::CNNNetwork> IEWrapper::mNetworkSet; //init static variable

IEWrapper::IEWrapper(InferenceEngine::Core& ie, struct ieModel& model,
                     const std::string& deviceName, const size_t maxBatch,
                     bool isImgInput = true):
    mDevice(deviceName), mMaxBatch(maxBatch), mIsImageInput(isImgInput), ie(ie) {
    mModel = model.modelPath;
    mHasReshape = model.hasReshape;
    mStride = model.stride;
    std::string modelPath(model.modelPath);

    memset(mMd5, 0, sizeof(mMd5));

    if (mModel.empty()) return;

    mIsInitialized = false;
    modelFileMd5(mModel, mMd5);

    std::string xmlExtension = "xml";
    mIsIRFormat = (mModel.compare(mModel.size() - xmlExtension.size(), xmlExtension.size(), xmlExtension) == 0);

    if (!mIsIRFormat) {
        std::cout << "not ir format" << std::endl;
        return;
    }

    if (mNetworkSet.find(modelPath) == mNetworkSet.end()) {
        std::cout << "new model" << std::endl;
        char *xml_data = NULL;
        uint8_t *bin_data = NULL;
        size_t bin_size = 0;
        char* key_file = NULL;
        char* eng_name = getenv("MODEL_KEY");
        if (eng_name && (eng_name = strdup(eng_name))) {
            if (!(key_file = strchr(eng_name, ':'))) {
                key_file = eng_name;
                eng_name = NULL;
            } else {
                *(key_file++) = '\0';
            }

            ENGINE *eng = load_crypto_engine(eng_name);
            EVP_PKEY* key = load_private_key(eng, key_file);
            free(eng_name ? eng_name : key_file);
            if (key) {
                std::string weightPath = modelPath.substr(0, modelPath.size() - 3) + "bin";
                int fd;
                if ((fd = open(modelPath.c_str(), O_RDONLY)) >= 0) {
                    xml_data = (char*)decrypt(eng, key, fd, &bin_size);
                    close(fd);
                }
                if ((fd = open(weightPath.c_str(), O_RDONLY)) >= 0) {
                    bin_data = (uint8_t*)decrypt(eng, key, fd, &bin_size);
                    close(fd);
                }
                EVP_PKEY_free(key);
            }
            ENGINE_finish(eng);
        }

        CNNNetwork network;
        if (xml_data && bin_data) {
            auto blob = make_shared_blob<uint8_t>({Precision::U8, {bin_size}, C}, bin_data);
            network = ie.ReadNetwork(std::string(xml_data), blob);
            free(xml_data);
            free(bin_data);
        } else {
            network = ie.ReadNetwork(modelPath);
        }
        mNetworkSet.insert({modelPath, network});
    }

    mNetwork = &mNetworkSet[modelPath];
    if (model.hasReshape) {
	reshape(model.aspectRatio, model.stride);
    }
    mNetwork->setBatchSize(maxBatch);
    configInputOutput();
}

IEWrapper::IEWrapper(InferenceEngine::Core& ie,
                   const std::string& modelPath,
                   const std::string& model,
                   const std::string& weights,
                   const std::string& deviceName,
                   size_t maxBatch,
                   bool isImgInput):
    mModel(modelPath), mDevice(deviceName), mMaxBatch(maxBatch), mIsImageInput(isImgInput), ie(ie) {

    if (mModel.empty()) return;

    mIsInitialized = false;
    mIsIRFormat = true;

    if (mNetworkSet.find(modelPath) == mNetworkSet.end()) {
        std::cout << "new model" << std::endl;
        CNNNetwork network = ie.ReadNetwork(
            model,
            make_shared_blob<uint8_t>(
               {Precision::U8, {weights.size()}, C}, (uint8_t *)weights.data())
            );
        mNetworkSet.insert({modelPath, network});
    }
    mNetwork = &mNetworkSet[modelPath];
    mNetwork->setBatchSize(maxBatch);
    configInputOutput();
}

IEWrapper::~IEWrapper() {
}

const std::vector<IOBlobInfo>& IEWrapper::readInputBlobInfo() const {
    return mInputBlobsInfo;
}

const std::vector<IOBlobInfo>& IEWrapper::readOutputBlobInfo() const {
    return mOutputBlobsInfo;
}

void IEWrapper::reshape(double aspectRatio, int stride) {
    if (aspectRatio == 0) {
        std::cout << "no need to reshape!" << std::endl;
	return;
    }

    ICNNNetwork::InputShapes inputShapes = mNetwork->getInputShapes();
    SizeVector& inputDims = inputShapes.begin()->second;
    int  targetSize =  static_cast<int>(std::min(inputDims[2], inputDims[3]));
    int inputHeight = static_cast<int>(std::round(targetSize * aspectRatio));
    int inputWidth = targetSize;
    if (aspectRatio >= 1.0) {
        std::swap(inputHeight, inputWidth);
    }
    int height = static_cast<int>((inputHeight + stride - 1) / stride) * stride;
    int width = static_cast<int>((inputWidth + stride - 1) / stride) * stride;

    inputDims[0] = 1;
    inputDims[2] = height;
    inputDims[3] = width;

    mNetwork->reshape(inputShapes);

    return;
}

void IEWrapper::configInputOutput() {
    InputsDataMap inputsInfo(mNetwork->getInputsInfo());

    for (auto& inputInfoItem : inputsInfo) {

        auto inputName = inputInfoItem.first;
        auto inputDims = inputInfoItem.second->getTensorDesc().getDims(); //getDims() return SizeVector = std::vector<size_t>

        std::vector<unsigned long> inputDims_(inputDims.data(), inputDims.data() + inputDims.size());//inputDims.data() is the pointer to the first item;
        mInputBlobsInfo.push_back({inputName, inputDims_});

        inputInfoItem.second->setLayout(inputInfoItem.second->getLayout());
        if (mIsImageInput && (inputDims.size() == 4)) {
            inputInfoItem.second->setPrecision(Precision::U8);
        } else {
            inputInfoItem.second->setPrecision(Precision::FP32);
        }
    }

    OutputsDataMap outputsInfo(mNetwork->getOutputsInfo());
    for (auto& outputInfoItem : outputsInfo) {
        auto outputName = outputInfoItem.first;
        auto outputDims = outputInfoItem.second->getTensorDesc().getDims();

        std::vector<unsigned long> outputDims_(outputDims.data(), outputDims.data() + outputDims.size());
        mOutputBlobsInfo.push_back({outputName, outputDims_});
        outputInfoItem.second->setPrecision(Precision::FP32);
    }

    if (!mDevice.empty()) {
        configExecPart(mDevice, 0);
    }

}

void IEWrapper::configExecPart(const std::string& deviceName, uint32_t gnaScalerFactor) {
    if (deviceName.empty() || mIsInitialized) {
        std::cout << "empty or initialized. no need config again" << std::endl;
        return;
    }

    mDevice = deviceName;
    std::string exeDevice = deviceName;
    std::map<std::string, std::string> pluginConfig;

    bool isGna = (mDevice.find("GNA") != std::string::npos) ? true : false;
    if (isGna) {
        bool isHetero = (mDevice.find("HETERO") != std::string::npos) ? true : false;
        std::string gnaDeviceName = isHetero ? "HETERO:GNA,CPU" : mDevice.substr(0, (mDevice.find("_")));

        std::map<std::string, std::string> gnaPluginConfig;
        std::string gnaDevice =
            isHetero ? mDevice.substr(mDevice.find("GNA"), mDevice.find(",") - mDevice.find("GNA")) : mDevice;
        gnaPluginConfig[GNAConfigParams::KEY_GNA_DEVICE_MODE] =
            gnaDevice.find("_") == std::string::npos ? "GNA_AUTO" : gnaDevice;
	if (gnaScalerFactor != 0)
            gnaPluginConfig[GNA_CONFIG_KEY(SCALE_FACTOR)] = std::to_string(gnaScalerFactor); //2048
	gnaPluginConfig[GNA_CONFIG_KEY(COMPACT_MODE)] = CONFIG_VALUE(NO);
        pluginConfig.insert(std::begin(gnaPluginConfig), std::end(gnaPluginConfig));

        exeDevice = gnaDeviceName;
    }

    mExeNetwork = mIsIRFormat ? ie.LoadNetwork(*mNetwork, exeDevice, pluginConfig) :
                                ie.ImportNetwork(mModel.c_str(), exeDevice, pluginConfig);
    mRequest = mExeNetwork.CreateInferRequest();

    if (!mIsIRFormat) {
        ConstInputsDataMap cInputInfo = mExeNetwork.GetInputsInfo();
        for (auto& input : cInputInfo) {
            auto blobPtr = mRequest.GetBlob(input.first);
            MemoryBlob::Ptr minput = as<MemoryBlob>(blobPtr);
            auto inputDims = minput->getTensorDesc().getDims();
            std::vector<unsigned long> inputDims_(inputDims.data(), inputDims.data() + inputDims.size());
            mInputBlobsInfo.push_back({input.first, inputDims_});
        }

        ConstOutputsDataMap cOutputInfo(mExeNetwork.GetOutputsInfo());
        for (auto& output : cOutputInfo) {
            auto blobPtr = mRequest.GetBlob(output.first);
            MemoryBlob::Ptr moutput = as<MemoryBlob>(blobPtr);
            auto outputDims = moutput->getTensorDesc().getDims();
            std::vector<unsigned long> outputDims_(outputDims.data(), outputDims.data() + outputDims.size());
            mOutputBlobsInfo.push_back({output.first, outputDims_});
        }
    }

    mIsInitialized = true;
}

std::vector<IOBlobInfo>::iterator IEWrapper::getIOInfo(std::vector<IOBlobInfo>& IOVector,
                                                      const std::string& blobName) {
    std::vector<IOBlobInfo>::iterator iter;
    for (iter = IOVector.begin(); iter != IOVector.end(); ++iter) {
        if (iter->first == blobName) {
            break;
        }
    }

    return iter;
}

int IEWrapper::modelFileMd5(const std::string &fileName, unsigned char* outMd) {

    if (!outMd) {
        std::cerr << "md5 buffer empty!" << std::endl;
        return -1;
    }

    #define MD5_BUFFER_SIZE  4096
    MD5_CTX ctx;
    MD5_Init(&ctx);

    std::streamsize length;
    char buffer[MD5_BUFFER_SIZE];

    std::ifstream in_file(fileName, std::ios::binary);
    if (in_file.good()) {
        while (!in_file.eof()) {
            in_file.read(buffer, MD5_BUFFER_SIZE);
            length = in_file.gcount();
            if (length > 0) {
              MD5_Update(&ctx, buffer ,length);
            }
        }
        in_file.close();
    }

    MD5_Final(outMd,&ctx);

    return 0;
}

bool IEWrapper::isModelChaged(const std::string &modelFile) {

    unsigned char newMd5[16];

    memset(newMd5, 0, sizeof(newMd5));
    modelFileMd5(modelFile, newMd5);

    bool changed = false;
    if(strncmp((char *)newMd5, (char *)mMd5, 16) != 0) {
        std::cerr << "model changed!" << std::endl;
        changed = true;
    }

    return changed;
}


void IEWrapper::removeNetworkFromMap() {
    if (mNetworkSet.find(mModel) != mNetworkSet.end()) {
        mNetworkSet.erase(mModel);
    }
}

void IEWrapper::writeInputData(const std::string& blobName,
                              const cv::Mat& image,
                              int batchIndex) {

    auto inputBlob = mRequest.GetBlob(blobName);

    int image_type = image.type() % 8;
    if (image_type < 5){
        matU8ToBlob<PrecisionTrait<Precision::U8>::value_type>(image, inputBlob, batchIndex);
    } else {
        matU8ToBlob<PrecisionTrait<Precision::FP32>::value_type>(image, inputBlob, batchIndex);
    }
}

void IEWrapper::writeInputData(const std::string& blobName,
                              const std::vector<float>& data) {
    auto iter = getIOInfo(mInputBlobsInfo, blobName);
    if (iter == mInputBlobsInfo.end()) return;
    auto& inputBlobDim = iter->second;

    unsigned long totalBlobSize = 1;
    for (auto& dim : inputBlobDim) {
        totalBlobSize *= dim;
    }

    if (totalBlobSize != data.size()) {
        throw std::runtime_error("Input data size not match size of the input blob");
    }

    auto inputBlob = mRequest.GetBlob(blobName);
    auto buffer = inputBlob->buffer().as<InferenceEngine::PrecisionTrait<InferenceEngine::Precision::FP32>::value_type *>();
    for (unsigned long int i = 0; i < data.size(); ++i) {
        buffer[i] = data[i];
    }
}

void IEWrapper::writeInputData(const std::string& blobName,
                              const float *data,
                              uint32_t batch) {
    if (!data) {
        throw std::runtime_error("Buffer empty");
    }
 
    auto iter = getIOInfo(mInputBlobsInfo, blobName);
    if (iter == mInputBlobsInfo.end()) return;
    auto& blobDims = iter->second;
    size_t vector_size = blobDims[1];
    size_t data_size = vector_size * batch;

    unsigned long dimsProduct = 1;
    for (auto& dim : blobDims) {
        dimsProduct *= dim;
    }

    if (dimsProduct < data_size) {
        throw std::runtime_error("Input data does not match size of the blob");
    }
    auto inputBlob = mRequest.GetBlob(blobName);
    auto buffer = inputBlob->buffer().as<InferenceEngine::PrecisionTrait<InferenceEngine::Precision::FP32>::value_type *>();
    for (unsigned long int i = 0; i < data_size; ++i) {
        buffer[i] = data[i];
    }
}

void IEWrapper::writeInputData(const std::string& blobName,
                              const std::vector<float>& input,
                              int batchIndex) {

    auto iter = getIOInfo(mInputBlobsInfo, blobName);
    if (iter == mInputBlobsInfo.end()) return;
    auto& blobDims = iter->second;
    unsigned long dimsProduct = 1;
    for (auto const& dim : blobDims) {
        dimsProduct *= dim;
    }

    if (dimsProduct != input.size()) {
        throw std::runtime_error("Input data does not match size of the blob");
    }

    size_t offset = dimsProduct * batchIndex;
    auto inputBlob = mRequest.GetBlob(blobName);
    auto buffer = inputBlob->buffer().as<InferenceEngine::PrecisionTrait<InferenceEngine::Precision::FP32>::value_type *>();
    for (unsigned long int i = 0; i < dimsProduct; ++i) {
        buffer[offset + i] = input[i];
    }
}

void IEWrapper::readOutputData(const std::string& blobName,
                              std::vector<float> &output) {
    output.clear();
    auto iter = getIOInfo(mOutputBlobsInfo, blobName);
    if (iter == mOutputBlobsInfo.end()) return;
    auto& blobDims = iter->second;
    auto totalBlobSize = 1;
    for (auto dim : blobDims) {
        totalBlobSize *= dim;
    }

    auto outputBlob = mRequest.GetBlob(blobName);
    auto buffer = outputBlob->buffer().as<InferenceEngine::PrecisionTrait<InferenceEngine::Precision::FP32>::value_type *>();

    for (int i = 0; i < totalBlobSize; ++i) {
        output.push_back(buffer[i]);
    }
}

void IEWrapper::readOutputData(const std::string& blobName,
                              std::vector<float>& output,
                              uint32_t batch) {

    auto iter = getIOInfo(mOutputBlobsInfo, blobName);
    if (iter == mOutputBlobsInfo.end()) return;
    auto& blobDims = iter->second;
    size_t vector_size = blobDims[1];
    int data_size = vector_size * batch;

    auto totalBlobSize = 1;
    for (auto dim : blobDims) {
        totalBlobSize *= dim;
    }

    data_size = std::min(data_size, totalBlobSize);

    auto outputBlob = mRequest.GetBlob(blobName);
    auto buffer = outputBlob->buffer().as<InferenceEngine::PrecisionTrait<InferenceEngine::Precision::FP32>::value_type *>();

    for (int i = 0; i < data_size; ++i) {
        output.push_back(buffer[i]);
    }
}

void IEWrapper::readOutputData(const std::string& blobName,
                               std::string& output,
                               uint32_t batch) {

    auto iter = getIOInfo(mOutputBlobsInfo, blobName);
    if (iter == mOutputBlobsInfo.end()) return;
    auto& blobDims = iter->second;
    size_t vector_size = blobDims[1];
    int data_size = vector_size * batch;

    auto totalBlobSize = 1;
    for (auto dim : blobDims) {
        totalBlobSize *= dim;
    }

    data_size = std::min(data_size, totalBlobSize);

    data_size *= sizeof(float);
    output.resize(data_size);

    auto outputBlob = mRequest.GetBlob(blobName);
    auto buffer = outputBlob->buffer().as<InferenceEngine::PrecisionTrait<InferenceEngine::Precision::FP32>::value_type *>();

    memcpy((void *)output.data(), (void *)buffer, data_size);
}

void IEWrapper::readInputWHAndScale(std::vector<float> &output) {
    output.clear();
    if (mHasReshape) {
        auto& inputBlobDim = mInputBlobsInfo[0].second;
        if (inputBlobDim.size() != 4) {
            std::cout << "dim size != 4, error!" << std::endl;
            return;
        }
        int inputHeight = inputBlobDim[2];
        int inputWidth = inputBlobDim[3];

	output.push_back(inputWidth);
	output.push_back(inputHeight);
    }

    return;
}

bool IEWrapper::isSingleInput() {
    if (mInputBlobsInfo.size() != 1) {
        std::cerr << "Error: have more than 1 input" << std::endl;
        return false;
    }

    return true;
}

bool IEWrapper::isSingleOutput() {
    if (mOutputBlobsInfo.size() != 1) {
        std::cerr << "Error: have more than 1 output" << std::endl;
        return false;
    }

    return true;
}
/* n should be smaller than size(), start from 0 */
std::string IEWrapper::getExpectInput(unsigned int n) const {
    if (n >= mInputBlobsInfo.size()) {
        throw std::runtime_error(mModel + ": no expected input");
    }
 
    return  mInputBlobsInfo[n].first;
}

/* n should be smaller than size(), start from 0 */
std::string IEWrapper::getExpectOutput(unsigned int n) const {
    if (n >= mOutputBlobsInfo.size()) {
        throw std::runtime_error(mModel + ": no expected output");
    }

    return  mOutputBlobsInfo[n].first;
}

/* n should be smaller than size(), start from 0 */
size_t IEWrapper::getExpectInputVectorSize(unsigned int n) const {
    if (n >= mInputBlobsInfo.size()) {
        std::cerr << mModel << ": no expected input" << std::endl;
        return 0;
    }

    return (mInputBlobsInfo[n].second.size() > 1) ?
            mInputBlobsInfo[n].second[1] : mInputBlobsInfo[n].second[0];
}

/* n should be smaller than size(), start from 0 */
size_t IEWrapper::getExpectOutputVectorSize(unsigned int n) const {
    if (n >= mOutputBlobsInfo.size()) {
        std::cerr << mModel << ": no expected output" << std::endl;
        return 0;
    }

    return (mOutputBlobsInfo[n].second.size() > 1) ?
            mOutputBlobsInfo[n].second[1] : mOutputBlobsInfo[n].second[0];
}

bool IEWrapper::isImageInput(const std::string& blobName) {
    auto iter = getIOInfo(mInputBlobsInfo, blobName);
    if (iter == mInputBlobsInfo.end()) return false;
    auto& dims = iter->second;

    if (dims.size() != 4 || dims[0] != 1 || dims[1] != 3) {
       std::cerr << mModel << ": expected " << blobName << " to have dimensions 1x3xHxW" << std::endl;
       return false;
    }

    return true;
}

void IEWrapper::infer() {
    //This is async interface
    mRequest.StartAsync();
    mRequest.Wait(IInferRequest::WaitMode::RESULT_READY);
}

StatusCode IEWrapper::infer(int64_t timeout) {
    //This is sync interface
    mRequest.Infer();
    return mRequest.Wait(timeout);
}

void IEWrapper::resetState() {
    for (auto &&state : mRequest.QueryState())
        state.Reset();
}

}  // namespace inference
