// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <vector>
#include <map>
#include <cstdio>
#include <string>

#include <opencv2/opencv.hpp>
#include "paddle_inference_api.h"


class PaddleWrapper {
public:
    PaddleWrapper() {};
    PaddleWrapper(const std::string& modelPath);
    // Write image data
    void writeInputData(const cv::Mat& image, uint32_t batchIndex);
    //inference
    void infer();
    //get output data
    void readOutputData(std::vector<float>& output, uint32_t batch);

private:
    std::string pModel;
    size_t mMaxBatch;
    // vector<float> tensor_input;
    // vector<float> tensor_output;
    paddle_infer::Config config;
    // paddle_infer::Predictor predictor;
    std::shared_ptr<paddle_infer::Predictor> predictor;
    
    std::unique_ptr<paddle_infer::Tensor> tensor_input;
    std::unique_ptr<paddle_infer::Tensor> tensor_output;
};
