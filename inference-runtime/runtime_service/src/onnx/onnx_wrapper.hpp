// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <onnxruntime_cxx_api.h>

namespace Ort {

class OrtWrapper {
 public:
    OrtWrapper(const std::string& modelPath,
               const std::string& deviceName, //used for future
               const size_t batch);
    ~OrtWrapper();

    int infer(const std::vector<float*>& inputData, std::vector<std::vector<float>*>& inferResults);

 private:
    void initOrtSession(const size_t batch);
    void configInputOutput(const size_t batch);

    std::string mModelPath;
    std::string mDevice;
    size_t      mBatch;

    Ort::Session mOrtSession;
    Ort::Env mOrtEnv;
    Ort::AllocatorWithDefaultOptions mOrtAllocator;

    //Input and output information
    uint8_t mNumInputs;
    uint8_t mNumOutputs;

    std::vector<char*> mInputNodeNames;
    std::vector<char*> mOutputNodeNames;

    std::vector<std::vector<int64_t>> mInputShapes; // the outer vector size means the input pins number. inner vector means shape
    std::vector<std::vector<int64_t>> mOutputShapes;

    std::vector<int64_t> mInputTensorSizes; //vector size means the input pins number
};

} //namespace of ort
