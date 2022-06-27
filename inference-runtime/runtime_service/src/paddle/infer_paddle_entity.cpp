// Copyright (C) 2021 Intel Corporation
//

#include <iostream>
#include <vector>

#include <syslog.h>

#include "paddle_wrapper.hpp"
#include "infer_paddle_entity.hpp"
#include<fstream>

infer_paddle_entity::infer_paddle_entity() {
    _inferEntityName = "PADDLE_INFER_ENTITY";
}

infer_paddle_entity::~infer_paddle_entity(){
}

std::string infer_paddle_entity::get_entity_name() const noexcept {
    return _inferEntityName;
}

void infer_paddle_entity::set_entity_name(const std::string& name) noexcept {
    _inferEntityName = name;
}

void infer_paddle_entity::set_config(const std::map<std::string, std::string>& config) {
    _config = config;
}

void infer_paddle_entity::get_config() {
    syslog(LOG_ERR, "AI-Service-Framework: infer_paddle_entity.get_config() not implemented! \n");
}

int infer_paddle_entity::infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& inputImg,
                                    std::vector<std::vector<float>>& additionalInput,
                                    const std::string& modelPath,
                                    const std::string& device,
                                    std::vector<std::vector<float>*>& rawDetectionResults) noexcept {
    PaddleWrapper paddle_wrapper(modelPath);
    cv::Mat img = inputImg[0][0]->clone();
    paddle_wrapper.writeInputData(img, 1);
    paddle_wrapper.infer();
    paddle_wrapper.readOutputData(*rawDetectionResults[0], 1);
    return 0;

}

int infer_paddle_entity::infer_speech(const short* samples, int sampleLength, int bytesPerSample,
                                     std::string config_path, const std::string& deviceName,
                                     std::vector<char> &rh_utterance_transcription) noexcept {
    syslog(LOG_ERR, "AI-Service-Framework: infer_paddle_entity.infer_speech() not implemented! \n");
    return 0;
}

int infer_paddle_entity::infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                                     std::vector<std::vector<float>>& additionalInput,
                                     std::string xmls, const std::string& device,
                                     std::vector<std::vector<float>*>& rawDetectionResults) noexcept {
    syslog(LOG_ERR, "AI-Service-Framework: infer_paddle_entity.infer_common() not implemented! \n");
    return 0;
}

int infer_paddle_entity::video_infer_init(struct modelParams& modelFile, const std::string& deviceName, struct mock_data& IOInformation) {
    syslog(LOG_ERR, "AI-Service-Framework: infer_paddle_entity.video_infer_init() not implemented! \n");
    return 0;
};

int infer_paddle_entity::video_infer_frame(const cv::Mat& frame,
                                          std::vector<std::vector<float>>& additionalInput,
                                          const std::string& modelFile,
                                          std::vector<std::vector<float>*>& rawDetectionResults) {
    syslog(LOG_ERR, "AI-Service-Framework: infer_paddle_entity.video_infer_frame() not implemented! \n");
    return 0;
};

