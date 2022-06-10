// Copyright (C) 2020 Intel Corporation
//

#include <memory>
#include <string>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>

#include "vino_ie_pipe.hpp"

namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(std::vector<char>);
PYBIND11_MAKE_OPAQUE(std::vector<float>);
PYBIND11_MAKE_OPAQUE(std::vector<std::vector<float>>);
PYBIND11_MAKE_OPAQUE(std::vector<std::vector<std::vector<float>>>);
PYBIND11_MAKE_OPAQUE(serverParams);
PYBIND11_MAKE_OPAQUE(userCfgParams);

int py_ie_pipeline_infer_image(std::vector<std::vector<uint8_t>>& image,
                               int image_channel,
                               std::vector<std::vector<float>>& additionalInput,
                               std::string xmls,
                               std::vector<std::vector<float>>& localInferResults,
                               struct serverParams& remoteServerInfo) {

    std::vector<std::shared_ptr<cv::Mat>> images;
    auto cv_flag = image_channel == 1 ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR;

    //Currently, the data format of image must be cv::imencode() or read from image file(jpg, png etc.) directly.
    for (size_t idx = 0; idx < image.size(); idx++) {
        std::shared_ptr<cv::Mat> frame_ptr = std::make_shared<cv::Mat>(cv::imdecode(image[idx], cv_flag));
        images.push_back(frame_ptr);
    }

    std::vector<std::vector<float>*> rawDetectionResults;
    for (size_t idx = 0; idx < localInferResults.size(); idx++)
        rawDetectionResults.push_back(&localInferResults[idx]);

    int res = vino_ie_pipeline_infer_image(images, additionalInput, xmls, rawDetectionResults, remoteServerInfo);

    return res;
}

int py_infer_image(std::vector<std::vector<std::vector<uint8_t>>>& images,
                   int image_channel,
                   std::vector<std::vector<float>>& additionalInput,
                   std::string xmls, std::string backendEngine,
                   std::vector<std::vector<float>>& localInferResults,
                   struct serverParams& remoteServerInfo) {

    auto cv_flag = image_channel == 1 ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR;

    std::vector<std::vector<ptrCVImage>> images_vecs;
    for (size_t pin = 0; pin < images.size(); pin++) {
        std::vector<std::shared_ptr<cv::Mat>> image_batch;
        for (size_t idx = 0; idx < images[pin].size(); idx++) {
            std::shared_ptr<cv::Mat> frame_ptr = std::make_shared<cv::Mat>(cv::imdecode(images[pin][idx], cv_flag));
            image_batch.push_back(frame_ptr);
        }
        images_vecs.push_back(image_batch);
    }

    std::vector<std::vector<float>*> rawDetectionResults;
    for (size_t idx = 0; idx < localInferResults.size(); idx++)
        rawDetectionResults.push_back(&localInferResults[idx]);

    struct irtImageIOBuffers buffers{&images_vecs, &additionalInput, &rawDetectionResults};
    int res = irt_infer_from_image(buffers, xmls, backendEngine, remoteServerInfo);

    return res;
}

int py_infer_speech(std::vector<short>& samples, int bytesPerSample,
                    std::string config_path, std::string backendEngine,
                    std::vector<char> &rh_utterance_transcription,
                    struct serverParams& remoteServerInfo) {

    short* sampleData = reinterpret_cast<short*>(samples.data());
    int dataLen = samples.size() * bytesPerSample;

    struct irtWaveData buffers{sampleData, dataLen, bytesPerSample};
    int res = irt_infer_from_speech(buffers, config_path, rh_utterance_transcription, backendEngine, remoteServerInfo);

    return res;
}

int py_ie_pipeline_live_asr(int mode,
                            std::vector<short>& samples,
                            int bytesPerSample,  // =2 if 16Kbits
                            std::string config_path,
                            std::string device,
                            std::vector<char> &rh_utterance_transcription) {

    short* sampleData = reinterpret_cast<short*>(samples.data());
    int dataLen = samples.size() * bytesPerSample;

    int res = vino_ie_pipeline_live_asr(mode, sampleData, dataLen, bytesPerSample, config_path,
                                        device, rh_utterance_transcription);

    return res;
}

int py_ie_pipeline_infer_tts(std::vector<std::vector<float>>& inputData,
                             std::vector<std::vector<float>>& additionalInput,
                             std::string xmls,
                             std::vector<std::vector<float>>& localInferResults,
                             struct serverParams& remoteServerInfo) {

    std::vector<std::shared_ptr<std::vector<float>>> inputDataPtr;

    for (size_t idx = 0; idx < inputData.size(); idx++) {
        std::shared_ptr<std::vector<float>> utt_ptr = std::make_shared<std::vector<float>>(inputData[idx]);
        inputDataPtr.push_back(utt_ptr);
    }

    std::vector<std::vector<float>*> rawDetectionResults;
    for (size_t idx = 0; idx < localInferResults.size(); idx++) {
        rawDetectionResults.push_back(&localInferResults[idx]);
    }

    int res = vino_ie_pipeline_infer_common(inputDataPtr, additionalInput, xmls, rawDetectionResults, remoteServerInfo);

    return res;
}

int py_infer_common(std::vector<std::vector<std::vector<float>>>& inputDatas,
                    std::vector<std::vector<float>>& additionalInput,
                    std::string xmls, std::string backendEngine,
                    std::vector<std::vector<float>>& localInferResults,
                    struct serverParams& remoteServerInfo) {

    std::vector<std::vector<ptrFloatVector>> mainInputs;

    for (size_t pin = 0; pin < inputDatas.size(); pin++) {
        std::vector<ptrFloatVector> inputDataPtr;
        for (size_t idx = 0; idx < inputDatas[pin].size(); idx++) {
            std::shared_ptr<std::vector<float>> utt_ptr = std::make_shared<std::vector<float>>(inputDatas[pin][idx]);
            inputDataPtr.push_back(utt_ptr);
        }
        mainInputs.push_back(inputDataPtr);
    }

    std::vector<std::vector<float>*> rawDetectionResults;
    for (size_t idx = 0; idx < localInferResults.size(); idx++) {
        rawDetectionResults.push_back(&localInferResults[idx]);
    }

    struct irtFloatIOBuffers buffers{&mainInputs, &additionalInput, &rawDetectionResults};
    int res = irt_infer_from_common(buffers, xmls, backendEngine, remoteServerInfo);

    return res;
}

// wrap as Python module
PYBIND11_MODULE(inferservice_python, m)
{
    m.doc() = "runtime service python binding";

    py::class_<serverParams>(m, "serverParams")
        .def(py::init<const std::string &, const std::string &, const std::string &>())
        .def(py::init<>())
        .def_readwrite("url", &serverParams::url)
        .def_readwrite("urlParam", &serverParams::urlParam)
        .def_readwrite("response", &serverParams::response);

    py::class_<userCfgParams>(m, "userCfgParams")
        .def(py::init<>())
        .def_readwrite("isLocalInference", &userCfgParams::isLocalInference)
        .def_readwrite("inferDevice", &userCfgParams::inferDevice);

    py::bind_vector<std::vector<char>>(m, "vectorChar", py::buffer_protocol());
    py::bind_vector<std::vector<float>>(m, "vectorFloat", py::buffer_protocol());
    py::bind_vector<std::vector<std::vector<float>>>(m, "vectorVecFloat");
    py::bind_vector<std::vector<std::vector<std::vector<float>>>>(m, "tripleVecFloat");

    m.def("init_from_buffer",  &vino_ie_pipeline_init_from_buffer,  "load model from buffer");
    m.def("infer_image",  &py_ie_pipeline_infer_image,  "image inference");
    m.def("infer_image_v1", &py_infer_image, "image inference supporting multiple image inputs");
    m.def("infer_speech", &py_infer_speech, "speech inference");
    m.def("live_asr", &py_ie_pipeline_live_asr, "live asr inference");
    m.def("infer_tts", &py_ie_pipeline_infer_tts, "text-to-speech inference");
    m.def("infer_common", &py_infer_common, "common case inference");
    m.def("set_policy_params", &vino_ie_pipeline_set_parameters, "parameters of policy configuration");
    m.def("set_temporary_infer_device", &irt_set_temporary_infer_device, "set or clear the temporary inference device");
}
