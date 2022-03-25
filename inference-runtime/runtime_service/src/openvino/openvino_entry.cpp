// Copyright (C) 2021 Intel Corporation
//

#include <vector>
#include <string>
#include <iostream>

#include <syslog.h>

#include "infer_openvino_entity.hpp"

extern "C" void CreatInferenceEntity(std::shared_ptr<IInfer_entity>& entity) {

    entity = std::make_shared<infer_openvino_entity>();

    std::string entity_name = entity->get_entity_name();
    std::cout << "entity name is:" << entity_name << std::endl;

    return;
}

extern "C" int vino_simulate_lib_init(std::shared_ptr<IInfer_entity>& entity,
                                      const std::string& modelXmlFile,
                                      uint32_t& batch, const std::string& deviceName,
                                      uint32_t gnaScaler,
                                      std::map<std::string, std::vector<unsigned long>>& inputBlobsInfo,
                                      std::map<std::string, std::vector<unsigned long>>& outputBlobsInfo) {

    std::shared_ptr<infer_openvino_entity> ov_entity = std::dynamic_pointer_cast<infer_openvino_entity>(entity);

    int res = -1;
    if (ov_entity != NULL)
        res = ov_entity->simulate_lib_init(modelXmlFile, batch, deviceName, gnaScaler, inputBlobsInfo, outputBlobsInfo);
    else
        syslog(LOG_ERR, "AI-Service-Framework: error to cast entity! \n");

    return res;
}

extern "C" int vino_simulate_lib_infer(std::shared_ptr<IInfer_entity>& entity,
                                       const std::string& audioData,
                                       const std::string& modelXmlFile,
                                       std::string& rawScoreResults) {

    std::shared_ptr<infer_openvino_entity> ov_entity = std::dynamic_pointer_cast<infer_openvino_entity>(entity);

    int res = -1;
    if (ov_entity != NULL)
        res = ov_entity->simulate_lib_infer(audioData, modelXmlFile, rawScoreResults);
    else
        syslog(LOG_ERR, "AI-Service-Framework: error to cast entity! \n");

    return res;
}

extern "C" int vino_init_from_buffer(std::shared_ptr<IInfer_entity>& entity,
                                     std::string xmls, const std::string &model,
                                     const std::string &weights, const std::string &deviceName,
                                     int batch, bool isImgInput) {

    std::shared_ptr<infer_openvino_entity> ov_entity = std::dynamic_pointer_cast<infer_openvino_entity>(entity);

    int res = -1;
    if (ov_entity != NULL)
        res = ov_entity->init_from_buffer(xmls, model, weights, deviceName, batch, isImgInput);
    else
        syslog(LOG_ERR, "AI-Service-Framework: error to cast entity! \n");

    return res;
}

extern "C" int vino_live_asr(std::shared_ptr<IInfer_entity>& entity,
                             int mode, const short* samples, int sampleLength,
                             int bytesPerSample, std::string config_path,
                             const std::string& deviceName,
                             std::vector<char> &rh_utterance_transcription) {

    std::shared_ptr<infer_openvino_entity> ov_entity = std::dynamic_pointer_cast<infer_openvino_entity>(entity);

    int res = -1;
    if (ov_entity != NULL)
        res = ov_entity->live_asr(mode, samples, sampleLength, bytesPerSample, config_path, deviceName, rh_utterance_transcription);
    else
        syslog(LOG_ERR, "AI-Service-Framework: error to cast entity! \n");

    return res;
}

