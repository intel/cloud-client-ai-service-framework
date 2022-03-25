// Copyright (C) 2021 Intel Corporation
//

#pragma once 

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "iinfer_entity.hpp"
#include "speech_library.h"

class infer_openvino_entity : public IInfer_entity {
  public:
    infer_openvino_entity();
    ~infer_openvino_entity();

    virtual int infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& images,
                            std::vector<std::vector<float>>& additionalInput,
                            const std::string& xmls, const std::string& device,
                            std::vector<std::vector<float>*>& rawDetectionResults) noexcept override;
    virtual int infer_speech(const short* samples, int sampleLength,
                             int bytesPerSample, std::string config_path,
                             const std::string& device,
                             std::vector<char> &rh_utterance_transcription) noexcept override;
    virtual int infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                             std::vector<std::vector<float>>& additionalInput,
                             std::string xmls, const std::string& device,
                             std::vector<std::vector<float>*>& rawDetectionResults) noexcept override;

    virtual std::string get_entity_name() const noexcept override;
    virtual void set_entity_name(const std::string& name) noexcept override;
    virtual void set_config(const std::map<std::string, std::string>& config) override;
    virtual void get_config() override; 

    virtual int video_infer_init(struct modelParams& modelInfo, const std::string& device, struct mock_data& IOInformation) override;
    virtual int video_infer_frame(const cv::Mat& frame, std::vector<std::vector<float>>& additionalInput,
                                  const std::string& modelXmlFile,
                                  std::vector<std::vector<float>*>& rawDetectionResults) override;

    int simulate_lib_init(const std::string& modelXmlFile, uint32_t& batch,
                          const std::string& device, uint32_t gnaScaler,
                          std::map<std::string, std::vector<unsigned long>>& inputBlobsInfo,
                          std::map<std::string, std::vector<unsigned long>>& outputBlobsInfo);
    int simulate_lib_infer(const std::string& audioData,
                           const std::string& modelXmlFile,
                           std::string& rawScoreResults);
    int init_from_buffer(std::string xmls, const std::string &model, const std::string &weights,
                         const std::string &device, int batch, bool isImgInput);
    int live_asr(int mode,  // 1 -- start  2 -- inference 0 -- stop
                 const short* samples,  int sampleLength,
                 int bytesPerSample, std::string config_path,
                 const std::string& device,
                 std::vector<char> &rh_utterance_transcription);

  private:
    int PushWaveData(SpeechLibraryHandle handle, const short* samples,
                     int sampleLength, int bytesPerSample, bool readResidueData);
};
