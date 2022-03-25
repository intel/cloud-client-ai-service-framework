// Copyright (C) 2020 Intel Corporation
//

#include <algorithm>
#include <numeric>
#include <iostream>

#include "onnx_wrapper.hpp"

namespace Ort {

OrtWrapper::OrtWrapper(
                     const std::string& modelPath,
                     const std::string& deviceName,
                     const size_t batch):
    mModelPath(modelPath), mDevice(deviceName), mBatch(batch),
    mOrtSession(nullptr), mOrtEnv(nullptr), mOrtAllocator(),
    mNumInputs(0), mNumOutputs(0), mInputNodeNames(),
    mOutputNodeNames(), mInputShapes(), mOutputShapes(),
    mInputTensorSizes() {

    initOrtSession(batch);
}

OrtWrapper::~OrtWrapper() {

    for (auto& elem : mInputNodeNames) {
        free(elem);
        elem = nullptr;
    }
    mInputNodeNames.clear();

    for (auto& elem : mOutputNodeNames) {
        free(elem);
        elem = nullptr;
    }
    mOutputNodeNames.clear();
}

void OrtWrapper::initOrtSession(const size_t batch) {

    mOrtEnv = Ort::Env(ORT_LOGGING_LEVEL_FATAL, "AI-Service-Framework");
    Ort::SessionOptions sessionOptions;

    // TODO: need to take care of the following line as it is related to CPU
    // consumption using openmp
    sessionOptions.SetIntraOpNumThreads(1);

    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    mOrtSession = Ort::Session(mOrtEnv, mModelPath.c_str(), sessionOptions);
    mNumInputs = mOrtSession.GetInputCount();

    mInputNodeNames.reserve(mNumInputs);   // no need to allocate memory when calling push_back
    mInputTensorSizes.reserve(mNumInputs);

    mNumOutputs = mOrtSession.GetOutputCount();

    mOutputNodeNames.reserve(mNumOutputs);

    configInputOutput(batch);
}

void OrtWrapper::configInputOutput(const size_t batch) {
    for (int i = 0; i < mNumInputs; i++) {
        Ort::TypeInfo typeInfo = mOrtSession.GetInputTypeInfo(i);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
        auto shape = tensorInfo.GetShape();

        shape[0] = batch > 0 ? batch : 1;
        mInputShapes.emplace_back(shape);

         //calculate tensor size from shape
        mInputTensorSizes.emplace_back(
            std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<int64_t>()));

        char* inputName = mOrtSession.GetInputName(i, mOrtAllocator);
        mInputNodeNames.emplace_back(strdup(inputName));
        mOrtAllocator.Free(inputName);
    }

    for (int i = 0; i < mNumOutputs; ++i) {
        Ort::TypeInfo typeInfo = mOrtSession.GetOutputTypeInfo(i);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();

        mOutputShapes.emplace_back(tensorInfo.GetShape());

        char* outputName = mOrtSession.GetOutputName(i, mOrtAllocator);
        mOutputNodeNames.emplace_back(strdup(outputName));
        mOrtAllocator.Free(outputName);
    }

}

int OrtWrapper::infer(const std::vector<float*>& inputData, std::vector<std::vector<float>*>& inferResults) {
    if (mNumInputs != inputData.size()) {
        std::cerr << "input buffer mismatch input pins" << std::endl;
        return -1;
    }

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<Ort::Value> inputTensors;
    inputTensors.reserve(mNumInputs);

    for (int i = 0; i < mNumInputs; ++i) {
        inputTensors.emplace_back(std::move(
            Ort::Value::CreateTensor<float>(memoryInfo, const_cast<float*>(inputData[i]), mInputTensorSizes[i],
                                            mInputShapes[i].data(), mInputShapes[i].size())));
    }

    auto outputTensors = mOrtSession.Run(Ort::RunOptions{nullptr}, mInputNodeNames.data(), inputTensors.data(),
                                       mNumInputs, mOutputNodeNames.data(), mNumOutputs);

    if ((outputTensors.size() != mNumOutputs) || (inferResults.size() != mNumOutputs)) {
        std::cerr << "output buffer mismatch output pins" << std::endl;
        return -1;
    }

    int count = 0;
    for (auto& elem : outputTensors) {
        std::vector<float>* outVec = inferResults[count];
        const auto& curOutputShape = elem.GetTensorTypeAndShapeInfo().GetShape();
        int64_t tensorLength =
             std::accumulate(std::begin(curOutputShape), std::end(curOutputShape), 1, std::multiplies<int64_t>()); //calculate tensor size from shape
        outVec->reserve(tensorLength);
        float* outTensorData = elem.GetTensorMutableData<float>();
 
        for(int64_t i = 0; i < tensorLength; i++)
             outVec->emplace_back(*(outTensorData + i));

        count++; 
    }

    return 0;
}

}  // namespace Ort
