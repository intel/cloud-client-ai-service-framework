// Copyright (C) 2021 Intel Corporation
//

#pragma once

#include <memory>
#include <string>

#include "shared_lib_pointer.hpp"
#include "iinfer_entity.hpp"

class inference_entity : protected SharedLibPointer<IInfer_entity> {
    using SharedLibPointer<IInfer_entity>::SharedLibPointer;

  public:
    int infer_image(std::vector<std::vector<std::shared_ptr<cv::Mat>>>& inputImg,
                    std::vector<std::vector<float>>& additionalInput,
                    const std::string& modelPath, const std::string& device,
                    std::vector<std::vector<float>*>& rawDetectionResults ) {
                          int ret = -1;
                          if(_ptr) ret = _ptr->infer_image(inputImg, additionalInput, modelPath, device,
                                                           rawDetectionResults);

                          return ret;
                   } //_ptr is in SharedLibPointer<>, std::shared_ptr<T> _ptr
    int infer_speech(const short* samples, int sampleLength,
                     int bytesPerSample, std::string config_path,
                     const std::string& deviceName,
                     std::vector<char> &rh_utterance_transcription) {
                          int ret = -1;
                          if(_ptr) ret = _ptr->infer_speech(samples, sampleLength, bytesPerSample,
                                                            config_path, deviceName, rh_utterance_transcription);

                          return ret;
                   }
    int infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                     std::vector<std::vector<float>>& additionalInput,
                     std::string xmls, const std::string& device,
                     std::vector<std::vector<float>*>& rawDetectionResults) {
                          int ret = -1;
                          if(_ptr) ret = _ptr->infer_common(inputData, additionalInput, xmls,
                                                            device, rawDetectionResults);

                          return ret;
                   }

    int video_infer_init(struct modelParams& modelFile, const std::string& deviceName, struct mock_data& IOInformation) {

                          int ret = -1;
                          if(_ptr) ret = _ptr->video_infer_init(modelFile, deviceName, IOInformation);

                          return ret;
                   }

    int video_infer_frame(const cv::Mat& frame, std::vector<std::vector<float>>& additionalInput,
                          const std::string& modelFile,
                          std::vector<std::vector<float>*>& rawDetectionResults) {

                          int ret = -1;
                          if(_ptr) ret = _ptr->video_infer_frame(frame, additionalInput, modelFile,
                                                                 rawDetectionResults);

                          return ret;
                   }

    void* findSymbol(std::string& symbol) {
        return findFuncAddress(symbol); 
    }

    std::shared_ptr<IInfer_entity> findEntityPointer() {
        return getPointer();
    }

};
