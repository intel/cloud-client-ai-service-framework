// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <vector>
#include <map>
#include <cstdio>
#include <string>

#include <inference_engine.hpp>
#include <samples/ocv_common.hpp>

namespace InferenceEngine {

using IOBlobInfo = std::pair<std::string, std::vector<unsigned long>>;

struct ieModel {
    std::string modelPath;
    bool hasReshape; //可以用hook函数，设置一个callBack参数，用来实现不同的model的不同处理
    double aspectRatio;
    int stride;
};

class IEWrapper {
public:
    IEWrapper() {};
    IEWrapper(InferenceEngine::Core& ie, struct ieModel& model /*const std::string& modelPath*/,
             const std::string& deviceName, const size_t maxBatch, bool isImgInput);

    IEWrapper(InferenceEngine::Core& ie, const std::string& modelPath,
             const std::string& modelBuffer, const std::string& weightsBuffer,
             const std::string& deviceName, size_t maxBatch, bool isImgInput = true);
    ~IEWrapper();

    // Write float data to input blob
    void writeInputData(const std::string& blobName, const float *data, uint32_t batch);
    // Write image data to input blob
    void writeInputData(const std::string& blobName, const cv::Mat& image, int batchIndex);
    // Write float vector data to input blob
    void writeInputData(const std::string& blobName, const std::vector<float>& data);
    // Write float vector data with batchindex to input blob
    void writeInputData(const std::string& blobName, const std::vector<float>& input, int batchIndex);

    // Read output data from output blob. Function overload to support batch output.
    void readOutputData(const std::string& blobName, std::vector<float>& output);
    void readOutputData(const std::string& blobName, std::vector<float>& output, uint32_t batch);
    void readOutputData(const std::string& blobName, std::string& output, uint32_t batch);

    bool isSingleInput();
    bool isSingleOutput();

    const std::vector<IOBlobInfo>& readInputBlobInfo() const;
    const std::vector<IOBlobInfo>& readOutputBlobInfo() const;

    std::string getExpectInput(unsigned int n) const;
    std::string getExpectOutput(unsigned int n) const;

    size_t getExpectInputVectorSize(unsigned int n) const;
    size_t getExpectOutputVectorSize(unsigned int n) const;

    bool isImageInput(const std::string& blobName);
    void infer();
    StatusCode infer(int64_t timeout);
    void configExecPart(const std::string& deviceName, uint32_t gnaScalerFactor);
    void resetState();
    void removeNetworkFromMap();
    bool isModelChaged(const std::string &modelFile);
    void reshape(double aspectRatio, int stride);
    void readInputWHAndScale(std::vector<float> &output);

    std::string getDeviceName() { return mDevice; };
    size_t getBatchSize() { return mMaxBatch; };
    bool isHasReshape() { return mHasReshape; };

private:
    void configInputOutput();
    std::vector<IOBlobInfo>::iterator getIOInfo(std::vector<IOBlobInfo>& IOVector,
                                                const std::string& blobName);
    int modelFileMd5(const std::string &fileName, unsigned char* outMd);

    size_t mMaxBatch;
    std::string mDevice;
    std::string mModel;
    bool mIsImageInput;
    bool mIsInitialized;
    bool mIsIRFormat;
    bool mHasReshape;
    int mStride;

    unsigned char mMd5[16];

    InferenceEngine::Core ie;
    InferenceEngine::InferRequest mRequest;
    InferenceEngine::CNNNetwork *mNetwork;
    InferenceEngine::ExecutableNetwork mExeNetwork;

    std::vector<IOBlobInfo> mInputBlobsInfo;
    std::vector<IOBlobInfo> mOutputBlobsInfo;

    static std::map<std::string, InferenceEngine::CNNNetwork> mNetworkSet;
};

}
