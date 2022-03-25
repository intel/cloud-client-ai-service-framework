// Copyright (C) 2021 Intel Corporation
//

#include <iostream>
#include <vector>

#include <syslog.h>

#include "tf_wrapper.hpp"
#include "infer_tf_entity.hpp"

infer_tf_entity::infer_tf_entity() {
    _inferEntityName = "TF_INFER_ENTITY";
}

infer_tf_entity::~infer_tf_entity(){
}

std::string infer_tf_entity::get_entity_name() const noexcept {
    return _inferEntityName;
}

void infer_tf_entity::set_entity_name(const std::string& name) noexcept {
    _inferEntityName = name;
}

void infer_tf_entity::set_config(const std::map<std::string, std::string>& config) {
    _config = config;
}

void infer_tf_entity::get_config() {
    syslog(LOG_ERR, "AI-Service-Framework: infer_tf_entity.get_config() not implemented! \n");
}

int infer_tf_entity::infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& inputImg,
                                    std::vector<std::vector<float>>& additionalInput,
                                    const std::string& modelPath,
                                    const std::string& device,
                                    std::vector<std::vector<float>*>& rawDetectionResults) noexcept {

    //ptrCVImage img = inputImg[0][0];
    std::shared_ptr<cv::Mat> img = inputImg[0][0];
    std::vector<float*> inputsData;
    inputsData.push_back(reinterpret_cast<float*>(img->data));

    TFWrapper tfInstance(modelPath);
    int res = tfInstance.infer(inputsData, rawDetectionResults);

    return (res == 0 ? 0 : -1);

}

int infer_tf_entity::infer_speech(const short* samples, int sampleLength, int bytesPerSample,
                                  std::string config_path, const std::string& deviceName,
                                  std::vector<char> &rh_utterance_transcription) noexcept {
    syslog(LOG_ERR, "AI-Service-Framework: infer_tf_entity.infer_speech() not implemented! \n");
    return 0;
}

int infer_tf_entity::infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                                  std::vector<std::vector<float>>& additionalInput,
                                  std::string xmls, const std::string& device,
                                  std::vector<std::vector<float>*>& rawDetectionResults) noexcept {
    syslog(LOG_ERR, "AI-Service-Framework: infer_tf_entity.infer_common() not implemented! \n");
    return 0;
}

int infer_tf_entity::video_infer_init(struct modelParams& modelFile, const std::string& deviceName, struct mock_data& IOInformation) {
    syslog(LOG_ERR, "AI-Service-Framework: infer_tf_entity.video_infer_init() not implemented! \n");
    return 0;
};

int infer_tf_entity::video_infer_frame(const cv::Mat& frame,
                                       std::vector<std::vector<float>>& additionalInput,
                                       const std::string& modelFile,
                                       std::vector<std::vector<float>*>& rawDetectionResults) {
    syslog(LOG_ERR, "AI-Service-Framework: infer_tf_entity.video_infer_frame() not implemented! \n");
    return 0;
};

