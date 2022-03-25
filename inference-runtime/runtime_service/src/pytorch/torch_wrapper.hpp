// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <vector>
#include <map>
#include <cstdio>
#include <string>

#include <opencv2/opencv.hpp>
#include <torch/script.h>

class TorchWrapper {
public:
    TorchWrapper() {};
    TorchWrapper(const std::string& modelPath);
    // Write image data
    void writeInputData(const cv::Mat& image, uint32_t batchIndex);
    //inference
    void infer();
    //get output data
    void readOutputData(std::vector<float>& output, uint32_t batch);

private:
    std::string mModel;
    size_t mMaxBatch;
    torch::Tensor tensor_input;
    torch::Tensor tensor_output;
    torch::jit::script::Module module;
};
