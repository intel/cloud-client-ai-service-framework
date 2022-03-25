// Copyright (C) 2021 Intel Corporation
//

#pragma once

#include <map>
#include <memory>
#include <string>

#include <opencv2/opencv.hpp>

#include "vino_ie_pipe.hpp"

class IInfer_entity : public std::enable_shared_from_this<IInfer_entity> {

  public:
    using Ptr = std::shared_ptr<IInfer_entity>;
    virtual int infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& inputImg,
                            std::vector<std::vector<float>>& additionalInput,
                            const std::string& modelPath, const std::string& device,
                            std::vector<std::vector<float>*>& rawDetectionResults) noexcept = 0;

    virtual int infer_speech(const short* samples, int sampleLength, int bytesPerSample,
                             std::string config_path, const std::string& deviceName,
                             std::vector<char> &rh_utterance_transcription) noexcept = 0;

    virtual int infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                             std::vector<std::vector<float>>& additionalInput,
                             std::string xmls, const std::string& device,
                             std::vector<std::vector<float>*>& rawDetectionResults) noexcept = 0;

    virtual int video_infer_init(struct modelParams& modelFile, const std::string& deviceName, struct mock_data& IOInformation) = 0;
    virtual int video_infer_frame(const cv::Mat& frame,
                                  std::vector<std::vector<float>>& additionalInput,
                                  const std::string& modelFile,
                                  std::vector<std::vector<float>*>& rawDetectionResults) = 0;

    virtual std::string get_entity_name() const noexcept;
    virtual void set_entity_name(const std::string& name) noexcept;
    virtual void set_config(const std::map<std::string, std::string>& config);
    virtual void get_config();

  protected:
    ~IInfer_entity() = default;

    std::string _inferEntityName;  //A entity name of inference backend
    std::map<std::string, std::string> _config;  //A map config keys 
};
