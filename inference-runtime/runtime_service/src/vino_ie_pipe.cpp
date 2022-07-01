// Copyright (C) 2020 Intel Corporation
//

#include <functional>
#include <iostream>
#include <fstream>
#include <random>
#include <memory>
#include <chrono>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <iterator>
#include <map>
#include <sys/wait.h>
#include <unistd.h>

#include "vino_ie_pipe.hpp"
#include "policy.hpp"
#include "connect_server.hpp"

#include "irt_inference_engine.hpp"

#include <ccai_log.h>

static irt_inference_backend gIrtInf;
static std::map<pid_t, std::map<std::string, std::string>> gIrtTempInferDevice;

int vino_ie_pipeline_set_parameters(struct userCfgParams& configuration) {

    Policy policyHandle;
    policyHandle.WakeupDaemon();
    if (!policyHandle.IsAvailableDevice(configuration.inferDevice)) {
        CCAI_ERROR("AI-Service-Framework: The specified device '%s' is not supported.",
                   configuration.inferDevice.c_str());
        return -1;
    }

    int res = policyHandle.UserSetCfgParams(configuration.isLocalInference, configuration.inferDevice);
    return res;
}

int irt_set_temporary_infer_device(bool set, const std::string& model, std::string device) {

    if (model.empty()) {
        return -1;
    }

    pid_t pid = getpid();

    if (set) {
        Policy policyHandle;
        if (!policyHandle.IsAvailableDevice(device)) {
            return -1;
        }

        if (gIrtTempInferDevice.find(pid) == gIrtTempInferDevice.end()) {
            std::map<std::string, std::string> tmp;
            tmp.insert(std::make_pair(model, device));
            gIrtTempInferDevice[pid] = tmp;
        } else {
            gIrtTempInferDevice[pid][model] = device;
        }
    } else {
        if ((gIrtTempInferDevice.find(pid) != gIrtTempInferDevice.end())
            && (gIrtTempInferDevice[pid].find(model) != gIrtTempInferDevice[pid].end())) {
            gIrtTempInferDevice[pid].erase(model);
        }
    }

    return 0;
}

static bool check_temporary_infer_device(const std::string& model, std::string& device) {

    bool res = false;
    pid_t pid = getpid();

    if ((gIrtTempInferDevice.find(pid) != gIrtTempInferDevice.end())
        && (gIrtTempInferDevice[pid].find(model) != gIrtTempInferDevice[pid].end())) {
        device = gIrtTempInferDevice[pid][model];
        res = true;
    }

    return res;
}

// This api only support image, not support video
// Return value: 0  - local ok
//               1  - remote server ok
//               -1 - error 
int vino_ie_pipeline_infer_image(std::vector<std::shared_ptr<cv::Mat>>& image,
                                 std::vector<std::vector<float>>& additionalInput,
                                 std::string xmls,
                                 std::vector<std::vector<float>*>& rawDetectionResults,
                                 struct serverParams& remoteServerInfo) {

    auto t0 = std::chrono::steady_clock::now();

    //policy checking, get deviceName
    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    bool toServer = false;
    if (policyParams.OffloadToServer && !remoteServerInfo.url.empty()) {
        if (!policyParams.OffloadToLocal || isXpuBusy.CPUBusy) {
            toServer = true;
        }
    }

    if (toServer) {
        CCAI_NOTICE("AI-Service-Framework: Must sendto server");
        HttpRequest post_req;
        int res = post_req.HttpPost(remoteServerInfo.url,
                                    remoteServerInfo.urlParam.c_str(),
                                    remoteServerInfo.response);

        return (res == -1 ? RT_INFER_ERROR : RT_REMOTE_INFER_OK);
    }

    std::string deviceName = policyParams.InferDevice; //GPU is slow when loading network
    //std::string deviceName = "HETERO:CPU,GPU"; //hetero is ok to loading network
    check_temporary_infer_device(xmls, deviceName);

    std::vector<std::vector<ptrCVImage>> images;
    images.push_back(image);

    std::string backendStr("OPENVINO");
    int res = RT_INFER_ERROR;
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    if (p_entity)
        res = p_entity->infer_image(images, additionalInput, xmls, deviceName, rawDetectionResults);

    auto t1 = std::chrono::steady_clock::now();

    CCAI_NOTICE("AI-Service-Framework: Inference API finished, infer time is: %ld ms!\n",
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    return res;
}

int vino_ie_pipeline_infer_image(std::vector<std::vector<ptrCVImage>>& images,
                                 std::vector<std::vector<float>>& additionalInput,
                                 std::string xmls,
                                 std::vector<std::vector<float>*>& rawDetectionResults,
                                 struct serverParams& remoteServerInfo) {

    auto t0 = std::chrono::steady_clock::now();

    //policy checking, get deviceName
    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    bool toServer = false;
    if (policyParams.OffloadToServer && !remoteServerInfo.url.empty()) {
        if (!policyParams.OffloadToLocal || isXpuBusy.CPUBusy) {
            toServer = true;
        }
    }

    if (toServer) {
        CCAI_NOTICE("AI-Service-Framework: Must sendto server");
        HttpRequest post_req;
        int res = post_req.HttpPost(remoteServerInfo.url,
                                    remoteServerInfo.urlParam.c_str(),
                                    remoteServerInfo.response);

        return (res == -1 ? RT_INFER_ERROR : RT_REMOTE_INFER_OK);
    }

    std::string deviceName = policyParams.InferDevice;
    check_temporary_infer_device(xmls, deviceName);

    std::string backendStr("OPENVINO");
    int res = RT_INFER_ERROR;
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    if (p_entity)
        res = p_entity->infer_image(images, additionalInput, xmls, deviceName, rawDetectionResults);

    auto t1 = std::chrono::steady_clock::now();

    CCAI_NOTICE("AI-Service-Framework: Inference API finished, infer time is: %ld ms!\n",
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    return res;
}

int vino_ie_pipeline_infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                                 std::vector<std::vector<float>>& additionalInput,
                                 std::string xmls,
                                 std::vector<std::vector<float>*>& rawDetectionResults,
                                 struct serverParams& remoteServerInfo) {

    auto t0 = std::chrono::steady_clock::now();

    //policy checking, get deviceName
    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    bool toServer = false;
    if (policyParams.OffloadToServer && !remoteServerInfo.url.empty()) {
        if (!policyParams.OffloadToLocal || isXpuBusy.CPUBusy) {
            toServer = true;
        }
    }

    if (toServer) {
        CCAI_NOTICE("AI-Service-Framework: Must sendto server");
        HttpRequest post_req;
        int res = post_req.HttpPost(remoteServerInfo.url,
                                    remoteServerInfo.urlParam.c_str(),
                                    remoteServerInfo.response);

        return (res == -1 ? RT_INFER_ERROR : RT_REMOTE_INFER_OK);
    }

    std::string deviceName = policyParams.InferDevice; //GPU is slow when loading network
    //std::string deviceName = "HETERO:CPU,GPU"; //hetero is ok to loading network
    check_temporary_infer_device(xmls, deviceName);

    std::string backendStr("OPENVINO");
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    int res = RT_INFER_ERROR;
    if (p_entity)
        res = p_entity->infer_common(inputData, additionalInput, xmls, deviceName, rawDetectionResults);

    auto t1 = std::chrono::steady_clock::now();

    CCAI_NOTICE("AI-Service-Framework: Inference API finished, infer time is: %ld ms!\n",
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    return res;
}


int vino_ie_pipeline_infer_speech(const short* samples,
                                  int sampleLength,
                                  int bytesPerSample,  // =2 if 16Kbits
                                  std::string config_path,
                                  std::vector<char> &rh_utterance_transcription,
                                  struct serverParams& remoteServerInfo) {

    auto t0 = std::chrono::steady_clock::now();

    //policy checking
    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    bool toServer = false;
    if (policyParams.OffloadToServer && !remoteServerInfo.url.empty()) {
        if (!policyParams.OffloadToLocal || isXpuBusy.CPUBusy) {
            toServer = true;
        }
    }

    if (toServer) {
        CCAI_NOTICE("AI-Service-Framework: Must sendto server!");
        HttpRequest post_req;
        int res = post_req.HttpPost(remoteServerInfo.url,
                                    remoteServerInfo.urlParam.c_str(),
                                    remoteServerInfo.response);

        return (res == -1 ? RT_INFER_ERROR : RT_REMOTE_INFER_OK);
    }

    std::string deviceName = policyParams.InferDevice;
    check_temporary_infer_device(config_path, deviceName);

    std::string backendStr("OPENVINO");
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    int res = RT_INFER_ERROR;
    if (p_entity)
        res = p_entity->infer_speech(samples, sampleLength, bytesPerSample, config_path,
                                     deviceName, rh_utterance_transcription);

    auto t1 = std::chrono::steady_clock::now();
    CCAI_NOTICE("AI-Service-Framework: Inference API finished, infer time is: %ld ms!\n",
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    return res;
}

int vino_simulate_ie_init(const std::string& modelXmlFile,
                          uint32_t& batch, //default=1
                          const std::string& deviceName,
                          uint32_t gnaScaler,
                          struct mock_data& IOInformation) {

    Policy policyHandle;
    policyHandle.WakeupDaemon();

    std::string backendStr("OPENVINO");
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    if (p_entity == nullptr) {
        CCAI_ERROR("AI-Service-Framework: can not find this entity!");
        return RT_INFER_ERROR;
    }

    auto entity = p_entity->findEntityPointer();

    int res = RT_INFER_ERROR;
    std::string func = "vino_simulate_lib_init";
    void* p = p_entity->findSymbol(func);
    if (p != nullptr) {
        using EntryF = int(std::shared_ptr<IInfer_entity>&, const std::string&,
                          uint32_t&, const std::string&, uint32_t,
                          std::map<std::string, std::vector<unsigned long>>&,
                          std::map<std::string, std::vector<unsigned long>>&);
        res = reinterpret_cast<EntryF*>(p)(entity, modelXmlFile, batch, deviceName, gnaScaler, IOInformation.inputBlobsInfo, IOInformation.outputBlobsInfo);
    } else {
        CCAI_ERROR("AI-Service-Framework: can not find function symbol - %s!", func.c_str());
    }

    return res;
}

int vino_simulate_ie_infer(const std::string& audioData,
                           const std::string& modelXmlFile,
                           std::string& rawScoreResults) {

    //auto t0 = std::chrono::steady_clock::now();

    std::string backendStr("OPENVINO");
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    if (p_entity == nullptr) {
        return RT_INFER_ERROR;
    }

    auto entity = p_entity->findEntityPointer();

    int res = RT_INFER_ERROR;
    std::string func = "vino_simulate_lib_infer";
    void* p = p_entity->findSymbol(func);
    if (p != nullptr) {
        using EntryF = int(std::shared_ptr<IInfer_entity>&,
                           const std::string&,
                           const std::string&,
                           std::string&);
        res = reinterpret_cast<EntryF*>(p)(entity, audioData, modelXmlFile, rawScoreResults);
    } else {
        std::cerr << "can not find function - " << func << std::endl;
    }

    //auto t1 = std::chrono::steady_clock::now();
    //CCAI_NOTICE("AI-Service-Framework: Inference API finished, total(%u float) infer time is: %ld us!\n",
    //            audioData.size(), std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());

    return res;
}

/* Video APIs
 * Notice: (1)Thread safe not guarantee.
 *         (2)Inference one frame each time. batch size > 1 not support
 */
int vino_ie_video_infer_init(struct modelParams& model,
                             const std::string& deviceName,
			     struct mock_data& IOInformation) {

    std::string backendStr("OPENVINO");
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    int res = RT_INFER_ERROR;
    if (p_entity)
        res = p_entity->video_infer_init(model, deviceName, IOInformation);

    return res;
}

int vino_ie_video_infer_frame(const cv::Mat& frame,
                              std::vector<std::vector<float>>& additionalInput,
                              const std::string& modelXmlFile,
                              std::vector<std::vector<float>*>& rawDetectionResults) {

    std::string backendStr("OPENVINO");
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    int res = RT_INFER_ERROR;
    if (p_entity)
        res = p_entity->video_infer_frame(frame, additionalInput, modelXmlFile, rawDetectionResults);

    return res;
}

int vino_ie_pipeline_init_from_buffer(std::string xmls,
                                      const std::string &model,
                                      const std::string &weights,
                                      int batch,
                                      bool isImgInput) {

    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    std::string deviceName = policyParams.InferDevice;
    check_temporary_infer_device(xmls, deviceName);

    std::string backendStr("OPENVINO");
    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendStr);
    if (p_entity == nullptr) {
        CCAI_ERROR("AI-Service-Framework: can not find this entity!");
        return RT_INFER_ERROR;
    }
    auto entity = p_entity->findEntityPointer();

    int res = RT_INFER_ERROR;
    std::string func = "vino_init_from_buffer";
    void* p = p_entity->findSymbol(func);
    if (p != nullptr) {
        using EntryF = int(std::shared_ptr<IInfer_entity>&,
                           std::string,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           int, bool);
        res = reinterpret_cast<EntryF*>(p)(entity, xmls, model, weights, deviceName, batch, isImgInput);
    } else {
        CCAI_ERROR("AI-Service-Framework: can not find function symbol - %s!", func.c_str());
    }

    return res;
}



enum irtStatusCode irt_infer_from_image(struct irtImageIOBuffers& tensorData,
                                        const std::string& modelFile,
                                        std::string backendEngine,
                                        struct serverParams& remoteServerInfo) {

    // Policy checking to decide inference locally or remotely
    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    bool toServer = false;
    if (policyParams.OffloadToServer && !remoteServerInfo.url.empty()) {
        if (!policyParams.OffloadToLocal || isXpuBusy.CPUBusy) {
            toServer = true;
        }
    }

    if (toServer) {
        HttpRequest post_req;
        int res = post_req.HttpPost(remoteServerInfo.url,
                                    remoteServerInfo.urlParam.c_str(),
                                    remoteServerInfo.response);

        return (res == -1 ? RT_INFER_ERROR : RT_REMOTE_INFER_OK);
    }

    std::vector<std::vector<ptrCVImage>> images;
    std::vector<std::vector<float>> additionalInput;
    std::vector<std::vector<float>*> rawDetectionResults;

    std::vector<std::vector<ptrCVImage>> *pImageInputs = (tensorData.pImageInputs == nullptr)
                                     ? &images : tensorData.pImageInputs;
    std::vector<std::vector<float>> *pAdditionalInputs = (tensorData.pAdditionalInputs == nullptr)
                                     ? &additionalInput : tensorData.pAdditionalInputs;
    std::vector<std::vector<float>*> *pInferenceResult = (tensorData.pInferenceResult == nullptr)
                                     ? &rawDetectionResults : tensorData.pInferenceResult;

    int res = RT_INFER_ERROR;
    std::string inferDevice = policyParams.InferDevice;
    check_temporary_infer_device(modelFile, inferDevice);

    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendEngine);
    if (p_entity)
        res = p_entity->infer_image(*pImageInputs, *pAdditionalInputs, modelFile, inferDevice, *pInferenceResult); 

    return (enum irtStatusCode)res;
}

enum irtStatusCode irt_infer_from_speech(const struct irtWaveData& waveData,
                                         std::string configurationFile,
                                         std::vector<char>& inferenceResult,
                                         std::string backendEngine,
                                         struct serverParams& remoteServerInfo) {

    // Policy checking to decide inference locally or remotely
    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    bool toServer = false;
    if (policyParams.OffloadToServer && !remoteServerInfo.url.empty()) {
        if (!policyParams.OffloadToLocal || isXpuBusy.CPUBusy) {
            toServer = true;
        }
    }

    if (toServer) {
        HttpRequest post_req;
        int res = post_req.HttpPost(remoteServerInfo.url,
                                    remoteServerInfo.urlParam.c_str(),
                                    remoteServerInfo.response);

        return (res == -1 ? RT_INFER_ERROR : RT_REMOTE_INFER_OK);
    }

    int res = RT_INFER_ERROR;
    std::string inferDevice = policyParams.InferDevice;
    check_temporary_infer_device(configurationFile, inferDevice);

    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendEngine);
    if (p_entity)
        res = p_entity->infer_speech(waveData.samples, waveData.sampleLength,
                                     waveData.bytesPerSample, configurationFile,
                                     inferDevice, inferenceResult);

    return (enum irtStatusCode)res;
}

enum irtStatusCode irt_infer_from_common(struct irtFloatIOBuffers& tensorData,
                                         const std::string& modelFile,
                                         std::string backendEngine,
                                         struct serverParams& remoteServerInfo) {

    // Policy checking to decide inference locally or remotely
    Policy policyHandle;
    policyHandle.WakeupDaemon();
    struct CfgParams policyParams{true, false, "CPU", 500};
    struct XPUStatus isXpuBusy{false, false};
    policyHandle.ReadShareMemory(policyParams, isXpuBusy);

    bool toServer = false;
    if (policyParams.OffloadToServer && !remoteServerInfo.url.empty()) {
        if (!policyParams.OffloadToLocal || isXpuBusy.CPUBusy) {
            toServer = true;
        }
    }

    if (toServer) {
        HttpRequest post_req;
        int res = post_req.HttpPost(remoteServerInfo.url,
                                    remoteServerInfo.urlParam.c_str(),
                                    remoteServerInfo.response);

        return (res == -1 ? RT_INFER_ERROR : RT_REMOTE_INFER_OK);
    }

    std::vector<ptrFloatVector> mainInput;
    std::vector<std::vector<float>> additionalInput;
    std::vector<std::vector<float>*> rawDetectionResults;

    std::vector<ptrFloatVector> *pMainInputs = (tensorData.pMainInputs == nullptr)
                                               ? &mainInput : &(*tensorData.pMainInputs)[0];
    std::vector<std::vector<float>> *pAdditionalInputs = (tensorData.pAdditionalInputs == nullptr)
                                                         ? &additionalInput : tensorData.pAdditionalInputs;
    std::vector<std::vector<float>*> *pInferenceResult = (tensorData.pInferenceResult == nullptr)
                                                         ? &rawDetectionResults : tensorData.pInferenceResult;

    int res = RT_INFER_ERROR;

    std::string inferDevice = policyParams.InferDevice;
    check_temporary_infer_device(modelFile, inferDevice);

    inference_entity* p_entity = gIrtInf.getInferenceEntity(backendEngine);
    if (p_entity)
        res = p_entity->infer_common(*pMainInputs, *pAdditionalInputs, modelFile,
                                     inferDevice, *pInferenceResult);

    return (enum irtStatusCode)res;
}
