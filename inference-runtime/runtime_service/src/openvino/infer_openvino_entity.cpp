// Copyright (C) 2021 Intel Corporation
//

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

#include <inference_engine.hpp>
#include <syslog.h>

#include "ie_wrapper.hpp"
#include "infer_openvino_entity.hpp"

using namespace InferenceEngine;

const int SUCCESS_STATUS = 0;
const int ERROR_STATUS = -1;

static std::map<std::string, class IEWrapper> gEngines;

infer_openvino_entity::infer_openvino_entity() {
    _inferEntityName = "OPENVINO_INFER_ENTITY";
}

infer_openvino_entity::~infer_openvino_entity(){
}

std::string infer_openvino_entity::get_entity_name() const noexcept {
    return _inferEntityName;
}

void infer_openvino_entity::set_entity_name(const std::string& name) noexcept {
    _inferEntityName = name;
}

void infer_openvino_entity::set_config(const std::map<std::string, std::string>& config) {
    _config = config;
}

void infer_openvino_entity::get_config() {
    syslog(LOG_ERR, "AI-Service-Framework: infer_openvino_entity.get_config() not implemented! \n");
}

int infer_openvino_entity::infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& images,
                                       std::vector<std::vector<float>>& additionalInput,
                                       const std::string& xmls, const std::string& device,
                                       std::vector<std::vector<float>*>& rawDetectionResults) noexcept {

    std::string deviceName("CPU");
    if (!device.empty())
        deviceName = device;

    if (images.empty() || rawDetectionResults.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: Input buffer or output buffer empty! \n");
        return -1;
    }

    size_t batch = std::min(images[0].size(), size_t(16));

    struct ieModel model{xmls, false, 0, 0};
    if (xmls.find("human-pose-estimation-0007") != std::string::npos) {
        cv::Mat& curr_frame = *images[0][0];
        double aspectRatio = curr_frame.cols / static_cast<double>(curr_frame.rows);

        model.hasReshape = true;
        model.aspectRatio = aspectRatio;
	model.stride = 32;
    }

    if (gEngines.find(xmls) == gEngines.end()) {
        Core ie;
        gEngines.insert({xmls, IEWrapper(ie, model, deviceName, batch, true)});
    } else if (gEngines[xmls].getDeviceName() != deviceName) {
        gEngines.erase(xmls);
        Core ie;
        gEngines.insert({xmls, IEWrapper(ie, model, deviceName, batch, true)});
    }

    auto& ieWrap = gEngines[xmls];
    const auto& inputInfo = ieWrap.readInputBlobInfo();
    const auto& outputInfo = ieWrap.readOutputBlobInfo();

    if (inputInfo.empty() || outputInfo.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: Model hasn't input or output! \n");
        return -1;
    }
    if ((inputInfo.size() != images.size() + additionalInput.size()) ||
        (outputInfo.size() > rawDetectionResults.size())) {
        syslog(LOG_ERR, "AI-Service-Framework: Mismatch between network ports and data buffers! \n");
        return -1;
    }
    size_t imageIndex = 0;
    size_t additionalIndex = 0;
    for (auto& input : inputInfo) {
        if ((input.second.size() == 4) && (imageIndex < images.size())) {
            batch = std::min(images[imageIndex].size(), size_t(16));
            for (size_t idx = 0; idx < batch; idx++) // batch: more than 1
                ieWrap.writeInputData(input.first, *images[imageIndex][idx], idx);
            imageIndex++;
        } else if(additionalIndex < additionalInput.size()){ //check data.size == vector.size??
            ieWrap.writeInputData(input.first, additionalInput[additionalIndex]);
            additionalIndex++;
        } else {
            syslog(LOG_ERR, "AI-Service-Framework: Error writting input data! \n");
            return -1;
        }
    }

    ieWrap.infer();

    for (size_t idx = 0; idx < outputInfo.size(); idx++) {
        ieWrap.readOutputData(outputInfo[idx].first, *rawDetectionResults[idx]);
    }

    // this is for hack code
    if(model.hasReshape) {
        size_t bufferNumber = rawDetectionResults.size();
	size_t ioNumber = outputInfo.size();

        if((bufferNumber - ioNumber) != (ioNumber + 1)) {
            std::cout << "extra buffer number is error!" << std::endl;
	    return -1;
	}
        for(size_t idx = 0; idx < ioNumber; idx++) {
           rawDetectionResults[ioNumber + idx]->assign(outputInfo[idx].second.begin(), outputInfo[idx].second.end());
	}
        ieWrap.readInputWHAndScale(*rawDetectionResults[bufferNumber - 1]);
    }

    return 0;
}

int infer_openvino_entity::infer_speech(const short* samples, int sampleLength,
                                        int bytesPerSample, std::string config_path,
                                        const std::string& device,
                                        std::vector<char> &rh_utterance_transcription) noexcept {

    return 0;

}

int infer_openvino_entity::infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                                        std::vector<std::vector<float>>& additionalInput,
                                        std::string xmls, const std::string& device,
                                        std::vector<std::vector<float>*>& rawDetectionResults) noexcept {

    if (inputData.empty() || rawDetectionResults.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: Input buffer or output buffer empty!\n");
        return -1;
    }

    std::string deviceName("CPU");
    if (!device.empty())
        deviceName = device;

    size_t batch = std::min(inputData.size(), size_t(16));

    if (gEngines.find(xmls) == gEngines.end()) {
        Core ie;
	struct ieModel model{xmls, false, 0, 0};
        gEngines.insert({xmls, IEWrapper(ie, model, deviceName, batch, false)});
    } else if (gEngines[xmls].getDeviceName() != deviceName) {
        gEngines.erase(xmls);
        Core ie;
	struct ieModel model{xmls, false, 0, 0};
        gEngines.insert({xmls, IEWrapper(ie, model, deviceName, batch, false)});
    }

    auto& ieWrap = gEngines[xmls];
    const auto& inputInfo = ieWrap.readInputBlobInfo();

    if (inputInfo.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: the model hasn't input!\n");
        return -1;
    }

    // the image input should be the first one. start from 0.
    auto imageBlobName = ieWrap.getExpectInput(0);

    int additionalInputNum = 0;
    if ((inputInfo.size() > 1) && (additionalInput.size() > 0)) {
        if (inputInfo.size() != (additionalInput.size() + 1))
            syslog(LOG_ERR, "AI-Service-Framework: Warning: the model inputs and inputs data is mismatch\n");
        additionalInputNum = std::min(inputInfo.size() - 1, additionalInput.size());
    }

    const auto& outputInfo = ieWrap.readOutputBlobInfo();
    if (outputInfo.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: the model hasn't output!\n");
        return -1;
    }
    int outputNum = std::min(outputInfo.size(), rawDetectionResults.size());

    for (size_t idx = 0; idx < batch; idx++) // batch: more than 1
        ieWrap.writeInputData(imageBlobName, *inputData[idx], idx);

    for (int i = 1; i <= additionalInputNum; i++) {
        auto inputBlobName = ieWrap.getExpectInput(i);
        ieWrap.writeInputData(inputBlobName, additionalInput[i-1]);
    }

    ieWrap.infer();

    for (int i = 0; i < outputNum; i++) {
        const auto& outputBlobName = ieWrap.getExpectOutput(i);
        ieWrap.readOutputData(outputBlobName, *rawDetectionResults[i]);
    }

    return 0;
}

int infer_openvino_entity::video_infer_init(struct modelParams& modelInfo,
                                            const std::string& device,
					    struct mock_data& IOInformation) {

    std::string deviceName("CPU");
    if (!device.empty())
        deviceName = device;

    std::string modelXmlFile(modelInfo.modelXmlFile);
    if (gEngines.find(modelXmlFile) == gEngines.end()) {
        Core ie;
	struct ieModel model;
	model.modelPath = modelXmlFile;
	model.hasReshape = modelInfo.needReshape;
	model.aspectRatio = modelInfo.aspectRatio;
	model.stride = modelInfo.stride,

        gEngines.insert({modelXmlFile, IEWrapper(ie, model, deviceName, 1, true)});
    }
    auto& ieInstance = gEngines[modelXmlFile];

    IOInformation.inputBlobsInfo.clear();
    auto& inputInfo = ieInstance.readInputBlobInfo();
    for (auto& inputItem : inputInfo) {
        std::vector<unsigned long> inputDims_(inputItem.second.data(), inputItem.second.data() + inputItem.second.size());
        IOInformation.inputBlobsInfo[inputItem.first] = inputDims_;
    }

    IOInformation.outputBlobsInfo.clear();
    auto& outputInfo = ieInstance.readOutputBlobInfo();
    for (auto& outputItem : outputInfo) {
        std::vector<unsigned long> outputDims_(outputItem.second.data(), outputItem.second.data() + outputItem.second.size());
        IOInformation.outputBlobsInfo[outputItem.first] = outputDims_;
    }

    return 0;
}

int infer_openvino_entity::video_infer_frame(const cv::Mat& frame,
                                             std::vector<std::vector<float>>& additionalInput,
                                             const std::string& modelXmlFile,
                                             std::vector<std::vector<float>*>& rawDetectionResults) {

    if (gEngines.find(modelXmlFile) == gEngines.end()) {
        syslog(LOG_ERR, "AI-Service-Framework: the video model doesn't been initialized!\n");
        return -1;
    }
    auto& ieWrap = gEngines[modelXmlFile];
    const auto& inputInfo = ieWrap.readInputBlobInfo();
    if (inputInfo.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: the model hasn't image input!\n");
        return -1;
    }
    // the image input should be the first one. start from 0.
    auto imageBlobName = ieWrap.getExpectInput(0);

    int additionalInputNum = 0;
    if ((inputInfo.size() > 1) && (additionalInput.size() > 0)) {
        if (inputInfo.size() != (additionalInput.size() + 1))
            syslog(LOG_WARNING, "Warning: model inputs and inputs dara is mismatch");
        additionalInputNum = std::min(inputInfo.size() - 1, additionalInput.size());
    }

    const auto& outputInfo = ieWrap.readOutputBlobInfo();
    if (outputInfo.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: the model hasn't image output!\n");
        return -1;
    }
    int outputNum = std::min(outputInfo.size(), rawDetectionResults.size());

    ieWrap.writeInputData(imageBlobName, frame, 0);

    for (int i = 1; i <= additionalInputNum; i++) {
        auto inputBlobName = ieWrap.getExpectInput(i);
        ieWrap.writeInputData(inputBlobName, additionalInput[i-1]);
    }

    ieWrap.infer();

    for (int i = 0; i < outputNum; i++) {
        const auto& outputBlobName = ieWrap.getExpectOutput(i);
        ieWrap.readOutputData(outputBlobName, *rawDetectionResults[i]);
    }

    return 0;
}

int infer_openvino_entity::simulate_lib_init(const std::string& modelXmlFile, uint32_t& batch,
                                             const std::string& device,
                                             uint32_t gnaScaler,
                                             std::map<std::string, std::vector<unsigned long>>& inputBlobsInfo,
                                             std::map<std::string, std::vector<unsigned long>>& outputBlobsInfo) {

    std::string deviceName("CPU");
    if (!device.empty()) {
        syslog(LOG_NOTICE, "AI-Service-Framework: the infer device is empty, use CPU as default device!\n");
        deviceName = device;
    }

    batch = (batch == 0) ? 1 : batch;
    batch = std::min(batch, uint32_t(8)); //???? The maximum batchsize of GNA is 8????

    if (gEngines.find(modelXmlFile) == gEngines.end()) {
        std::cerr << "New simulation model" << std::endl;
        Core ie;
	struct ieModel model{modelXmlFile, false, 0, 0};
        gEngines.insert({modelXmlFile, IEWrapper(ie, model, deviceName, batch, false)});
    } else if (gEngines[modelXmlFile].isModelChaged(modelXmlFile)) {
        std::cerr << "model is changed!" << std::endl;
        gEngines[modelXmlFile].removeNetworkFromMap();
        gEngines.erase(modelXmlFile);
        Core ie;
	struct ieModel model{modelXmlFile, false, 0, 0};
        gEngines.insert({modelXmlFile, IEWrapper(ie, model, deviceName, batch, false)});
    }

    auto& ieInstance = gEngines[modelXmlFile];

    if (!deviceName.empty()) {
        // create exec parts
        ieInstance.configExecPart(deviceName, gnaScaler);
        ieInstance.resetState();
    }

    inputBlobsInfo.clear();
    auto& inputInfo = ieInstance.readInputBlobInfo();
    for (auto& inputItem : inputInfo) {
        std::vector<unsigned long> inputDims_(inputItem.second.data(), inputItem.second.data() + inputItem.second.size());
        inputBlobsInfo[inputItem.first] = inputDims_;
    }

    outputBlobsInfo.clear();
    auto& outputInfo = ieInstance.readOutputBlobInfo();
    for (auto& outputItem : outputInfo) {
        std::vector<unsigned long> outputDims_(outputItem.second.data(), outputItem.second.data() + outputItem.second.size());
        outputBlobsInfo[outputItem.first] = outputDims_;
    }

    return 0;
}

int infer_openvino_entity::simulate_lib_infer(const std::string& audioData,
                                              const std::string& modelXmlFile,
                                              std::string& rawScoreResults) {

    if (audioData.empty()) {
        syslog(LOG_ERR, "AI-Service-Framework: Input buffer not empty!\n");
        return -1;
    }

    if (!rawScoreResults.empty()) {
        syslog(LOG_NOTICE, "AI-Service-Framework: Warning: output buffer not empty!\n");
        rawScoreResults.clear();
    }

    if (gEngines.find(modelXmlFile) == gEngines.end()) {
        syslog(LOG_ERR, "AI-Service-Framework: the speech model doesn't been initialized!\n");
        return -1;
     }

    auto& ieWrap = gEngines[modelXmlFile];
    uint32_t batch_size = ieWrap.getBatchSize();

    //currently only support the model with only one input and one output;
    if (!ieWrap.isSingleInput() || !ieWrap.isSingleOutput()) {
        syslog(LOG_ERR, "AI-Service-Framework: expect single input or single output!\n");
        return -1;
    }

    auto inputBlobName = ieWrap.getExpectInput(0);
    auto outputBlobName = ieWrap.getExpectOutput(0);

    size_t input_vector_size = ieWrap.getExpectInputVectorSize(0);

    input_vector_size = (input_vector_size == 0) ? 1 : input_vector_size; 
    size_t numberOfFrames = (audioData.size() / sizeof(float)) / input_vector_size;

    size_t frame_index = 0;
    StatusCode status = StatusCode::OK;
    while (frame_index < numberOfFrames) {

        uint32_t frames_to_compute_in_this_batch = (numberOfFrames - frame_index < batch_size) ?
                                                   (numberOfFrames - frame_index) : batch_size;

        //elements in the vector are guaranteed to be stored in contiguous storage locations in the same order
        // as represented by the vector, the pointer
        // retrieved can be offset to access any element in the array.
        float* input_index = (float*)(audioData.data()) + frame_index * input_vector_size;

        ieWrap.writeInputData(inputBlobName, input_index, frames_to_compute_in_this_batch);

        status = ieWrap.infer(0);
        if (status != StatusCode::OK) {
            syslog(LOG_ERR, "AI-Service-Framework: inference result code not ok. Break\n");
            break;
        }

        ieWrap.readOutputData(outputBlobName, rawScoreResults, frames_to_compute_in_this_batch);

        frame_index += frames_to_compute_in_this_batch;
    }

    return (int)(status);
}

int infer_openvino_entity::init_from_buffer(std::string xmls, const std::string &model,
                                            const std::string &weights,
                                            const std::string &device,
                                            int batch, bool isImgInput) {

    std::string deviceName("CPU");
    if (!device.empty()) {
        syslog(LOG_NOTICE, "AI-Service-Framework: the infer device is empty, use CPU as default device!\n");
        deviceName = device;
    }

    if (gEngines.find(xmls) == gEngines.end()) {
        Core ie;
        gEngines.insert({xmls, IEWrapper(ie, xmls, model, weights, deviceName, batch, isImgInput)});
    } else if (gEngines[xmls].getDeviceName() != deviceName) {
        gEngines.erase(xmls);
        Core ie;
        gEngines.insert({xmls, IEWrapper(ie, xmls, model, weights, deviceName, batch, isImgInput)});
    }

    return 0;
}

int infer_openvino_entity::live_asr(int mode, const short* samples,
                                    int sampleLength, int bytesPerSample,
                                    std::string config_path, const std::string& device,
                                    std::vector<char> &rh_utterance_transcription) {
    return 0;
}
