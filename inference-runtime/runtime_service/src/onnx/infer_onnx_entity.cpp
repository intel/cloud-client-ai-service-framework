// Copyright (C) 2021 Intel Corporation
//

#include <iostream>
#include <vector>

#include <syslog.h>

#include "onnx_wrapper.hpp"
#include "infer_onnx_entity.hpp"

infer_onnx_entity::infer_onnx_entity() {
    _inferEntityName = "ONNX_INFER_ENTITY";
}

infer_onnx_entity::~infer_onnx_entity(){
}

std::string infer_onnx_entity::get_entity_name() const noexcept {
    return _inferEntityName;
}

void infer_onnx_entity::set_entity_name(const std::string& name) noexcept {
    _inferEntityName = name;
}

void infer_onnx_entity::set_config(const std::map<std::string, std::string>& config) {
    _config = config;
}

void infer_onnx_entity::get_config() {
    syslog(LOG_ERR, "AI-Service-Framework: infer_onnx_entity.get_config() not implemented! \n");
}

int infer_onnx_entity::infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& inputImg,
                                   std::vector<std::vector<float>>& additionalInput,
                                   const std::string& modelPath, const std::string& device,
                                   std::vector<std::vector<float>*>& rawDetectionResults) noexcept {

    size_t imagePins = inputImg.size();
    if ((imagePins == 0) || (inputImg[0].size() == 0) || (rawDetectionResults.empty())) {
        syslog(LOG_ERR, "AI-Service-Framework: Input buffer output buffer empty! \n");
        return -1;
    }

    std::vector<float*> inputsData;
    std::vector<cv::Mat> images;
    int batch = inputImg[0].size();

    for (size_t pin = 0; pin < imagePins; pin++) {
        auto& oneImage = inputImg[pin][0];
        int imageNum = inputImg[pin].size(); //all batch number in different inputs should be same
        int imageType = oneImage->type();

        if ((imageNum == 1) && (imageType % 8 == 5)) {
            inputsData.push_back(reinterpret_cast<float*>(oneImage->data));
            continue;
        }

        // covert to float type (cv::Mat)
        int rows = oneImage->rows;
        int cols = oneImage->cols;

        int chans = 1 + (imageType >> CV_CN_SHIFT);

        cv::Mat allImageInBatch(rows * imageNum, cols, CV_MAKETYPE(CV_32F, chans)); // type);
        for (int item = 0; item < imageNum; item++) {
            inputImg[pin][item]->copyTo(allImageInBatch(cv::Rect(0, item * rows, cols, rows))); //put all images in the batch in one image;
        }

        images.push_back(allImageInBatch);
        inputsData.push_back(reinterpret_cast<float*>(allImageInBatch.data));
    }

    for (size_t pin = 0; pin < additionalInput.size(); pin++) {
        inputsData.push_back(additionalInput[pin].data());
    }

    std::string inferenceDevice = "CPU";
    if (!device.empty()) {
        inferenceDevice = device;
    }

    Ort::OrtWrapper onnxInstance(modelPath, inferenceDevice, batch);

    int res = onnxInstance.infer(inputsData, rawDetectionResults);

    return (res == 0 ? 0 : -1);

}

int infer_onnx_entity::infer_speech(const short* samples, int sampleLength,
                                    int bytesPerSample, std::string config_path,
                                    const std::string& deviceName,
                                    std::vector<char> &rh_utterance_transcription) noexcept {
    syslog(LOG_ERR, "AI-Service-Framework: infer_onnx_entity.infer_speech() not implemented! \n");
    return 0;
}

int infer_onnx_entity::infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                                    std::vector<std::vector<float>>& additionalInput,
                                    std::string xmls, const std::string& device,
                                    std::vector<std::vector<float>*>& rawDetectionResults) noexcept {
    syslog(LOG_ERR, "AI-Service-Framework: infer_onnx_entity.infer_common() not implemented! \n");
    return 0;
}

int infer_onnx_entity::video_infer_init(struct modelParams& modelFile, const std::string& deviceName, struct mock_data& IOInformation) {
    syslog(LOG_ERR, "AI-Service-Framework: infer_onnx_entity.vidio_infer_init() not implemented! \n");
    return 0;
};

int infer_onnx_entity::video_infer_frame(const cv::Mat& frame,
                                         std::vector<std::vector<float>>& additionalInput,
                                         const std::string& modelFile,
                                         std::vector<std::vector<float>*>& rawDetectionResults) {
    syslog(LOG_ERR, "AI-Service-Framework: infer_onnx_entity.video_infer_frame() not implemented! \n");
    return 0;
};
