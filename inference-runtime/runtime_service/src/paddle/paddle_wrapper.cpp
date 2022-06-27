// Copyright (C) 2020 Intel Corporation
//

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <syslog.h>
#include "paddle_wrapper.hpp"

std::string find_param(std::string &model_file) 
{
    return model_file.substr(0, model_file.length()-7)+"pdiparams";
}

PaddleWrapper::PaddleWrapper(const std::string& modelPath):pModel(modelPath) {
    std::string pParam = find_param(pModel);
    config.SetModel(pModel, pParam);
    
    //config.EnableTensorRtEngine(1 << 30, FLAGS_batch_size, 10, PrecisionType::kFloat32, false, false);
    predictor = paddle_infer::CreatePredictor(config);
}

void PaddleWrapper::writeInputData(const cv::Mat& image, uint32_t batchIndex){
    // tensor_input = torch::from_blob(
    //                     image.data, {1, image.cols, image.rows, image.channels()});
    // tensor_input = tensor_input.permute({0, 3, 1, 2});
    auto input_names = predictor->GetInputNames();
    tensor_input = predictor->GetInputHandle(input_names[0]);
    std::vector<int> INPUT_SHAPE = {1, image.channels(), image.cols, image.rows};
    tensor_input->Reshape(INPUT_SHAPE);
    std::vector<float> input_data(1* image.channels()* image.cols* image.rows);
    // make some shape change
    // your code here

    int rh = image.rows;
    int rw = image.cols;
    int rc = image.channels();
    for (int i = 0; i < rc; ++i) {
        cv::extractChannel(image, cv::Mat(rh, rw, CV_32FC1, input_data.data() + i * rh * rw), i);
    }
    // copy to tensor
    tensor_input->CopyFromCpu(input_data.data());
}

void PaddleWrapper::infer(){
    predictor->Run();
}

void PaddleWrapper::readOutputData(std::vector<float>& output, uint32_t batch){
    output.clear();
    auto output_names = predictor->GetOutputNames();
    tensor_output = predictor->GetOutputHandle(output_names[0]);
    std::vector<int> output_shape = tensor_output->shape();

    output.resize(output_shape[0] * output_shape[1]);
    tensor_output->CopyToCpu(output.data());
}
