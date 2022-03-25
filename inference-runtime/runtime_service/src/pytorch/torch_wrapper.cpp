// Copyright (C) 2020 Intel Corporation
//

#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include "torch_wrapper.hpp"

TorchWrapper::TorchWrapper(const std::string& modelPath):
    mModel(modelPath) {

    std::cout << "new torch model" <<std::endl;
    module = torch::jit::load(mModel);

}

void TorchWrapper::writeInputData(const cv::Mat& image, uint32_t batchIndex){
    tensor_input = torch::from_blob(
                        image.data, {1, image.cols, image.rows, image.channels()});
    tensor_input = tensor_input.permute({0, 3, 1, 2});

}

void TorchWrapper::infer(){
     tensor_output = module.forward({tensor_input}).toTensor();
}

void TorchWrapper::readOutputData(std::vector<float>& output, uint32_t batch){
    output.clear();
    tensor_output = torch::softmax(tensor_output, 1);
    std::vector<float> rawDetectionResult(tensor_output.data_ptr<float>(), tensor_output.data_ptr<float>() + tensor_output.numel());
    for (size_t  i = 0 ; i < tensor_output.numel(); i++){
        output.push_back(rawDetectionResult[i]);
    }
}
