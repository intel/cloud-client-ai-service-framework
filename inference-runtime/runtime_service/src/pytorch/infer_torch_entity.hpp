// Copyright (C) 2021 Intel Corporation
//

#pragma once 

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "iinfer_entity.hpp"

class infer_torch_entity : public IInfer_entity {
  public:
    infer_torch_entity();
    ~infer_torch_entity();

    virtual int infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& inputImg,
                            std::vector<std::vector<float>>& additionalInput,
                            const std::string& modelPath, const std::string& device,
                            std::vector<std::vector<float>*>& rawDetectionResults) noexcept override;
    virtual int infer_speech(const short* samples, int sampleLength, int bytesPerSample,
                             std::string config_path, const std::string& deviceName,
                             std::vector<char> &rh_utterance_transcription) noexcept override;
    virtual int infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                             std::vector<std::vector<float>>& additionalInput,
                             std::string xmls, const std::string& device,
                             std::vector<std::vector<float>*>& rawDetectionResults) noexcept override;

    virtual int video_infer_init(struct modelParams& modelFile, const std::string& deviceName, struct mock_data& IOInformation) override;
    virtual int video_infer_frame(const cv::Mat& frame,
                                  std::vector<std::vector<float>>& additionalInput,
                                  const std::string& modelFile,
                                  std::vector<std::vector<float>*>& rawDetectionResults) override;

    virtual std::string get_entity_name() const noexcept override;
    virtual void set_entity_name(const std::string& name) noexcept override;
    virtual void set_config(const std::map<std::string, std::string>& config) override;
    virtual void get_config() override; 
};
