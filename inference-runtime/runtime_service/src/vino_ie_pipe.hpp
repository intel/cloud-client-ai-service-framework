// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <list>
#include <string>
#include <utility>
#include <vector>
#include <map>

#include <opencv2/opencv.hpp>

/**
 * @brief A smart pointer to the CV image data
 */
using ptrCVImage = std::shared_ptr<cv::Mat>;
/**
 * @brief A smart pointer to the vector<float>
 */
using ptrFloatVector = std::shared_ptr<std::vector<float>>;

/**
 *@enum irtStatusCode
 *@brief Status code of running infernce
 */
enum irtStatusCode : int {
    RT_REMOTE_INFER_OK = 1,           //inference successfully on remote server
    RT_LOCAL_INFER_OK = 0,            //inference successfully on local HW
    RT_INFER_ERROR = -1               //inference error
};

/**
 * @brief Buffers for running inference from image.
 *        Includes pointers point to the input/output tensors data.
 * @There are two kind of inputs, one is image inputs, another is assistant inputs.
 *  The image input tensor is represents by vector<vector<vector_shared_ptr>>>,
 *  means [ports_number, batch, one_image_data]. It is expressed by
 *  <ports_number<batch<one_image_data>>>.
 *  The inner vector is a shared pointer points to a vector(one_image_data).
 *  The outer vector.size() means the number of image input ports.
 *  The middle vector means batch.
 *  The assistant input tensor is represent by vector<vector<float>>, means
 *  [ports_number, one_data_array].
 * @The output tensor is represents by vector<vector_pointers>.
 *  The output tensor is [ports_number, one_data_arry]. It is expressed by
 *  <ports_number<one_data_arry>>.
 *  The inner vector is a pointer points to a vector(one_data_array). This vector
 *  includes return value passed back by API.
 *  The outer vector.size() means output ports number of the model.
 */
struct irtImageIOBuffers {
    /* Pointer points to the image input data. The inner shared pointer points to CV:Mat data */
    std::vector<std::vector<ptrCVImage>> *pImageInputs;
    /* Pointer points to the assistant input data. */
    std::vector<std::vector<float>> *pAdditionalInputs;
    /* Pointer points to the output buffer. The inner pointer points to the result inferenced by runtime API */
    std::vector<std::vector<float>*> *pInferenceResult;
};

/**
 * @brief Parameters for wave data. Used by running inference for speech.
 *        Wave data is PCM data.
 */
struct irtWaveData {
    /* Pointer points to PCM data. */
    short* samples;
    /* PCM data length. Byte size */
    int sampleLength;
    /* Size of each PCM sample. How many bytes for each sample*/
    int bytesPerSample;
};

/*
 * @brief Buffers for running inference from common model.
 *        The structure is similar with irtImageIOBuffers, except the type of shared pointer is float,
 *        not CV::Mat.
 */
struct irtFloatIOBuffers {
    /* Pointer points to main input data. The inner shared pointer points to float data */
    std::vector<std::vector<ptrFloatVector>> *pMainInputs;
    /* Pointer points to the assistant input data. */
    std::vector<std::vector<float>> *pAdditionalInputs;
    /* Pointer points to the output buffer. The inner pointer points to the result inferenced by runtime API */
    std::vector<std::vector<float>*> *pInferenceResult;
};

/**
 * @brief This is the parameters to do reference on remote server
 */
struct serverParams {
    std::string url;       //the address of server  
    std::string urlParam;  //the post parameter of request
    std::string response;  //the response data of server
};

/**
 * @brief This is the parameters setting from end user
 */
struct userCfgParams {
    bool isLocalInference;    //do inference in local or remote
    std::string inferDevice;  //inference device: CPU, GPU or other device
};

/**
 * @brief Set parameters to configure vino ie pipeline
 * @param configuration Parameters set from the end user.
 */
int vino_ie_pipeline_set_parameters(struct userCfgParams& configuration);

/**
 * @brief Set temporary inference device for the model.
 * @      This setting will override the policy setting.
 * @param set    Set or clear the temporary inference device.
 * @param model  Name of the model file, including the file path.
 * @param device The temporary inference device.
 */
int irt_set_temporary_infer_device(bool set, const std::string& model, std::string device);

/**
 * @brief Do inference for image. For network with only one image input.
 * @param image Image input for network. Only have one image input.
 * @param additionalInput Other inputs of network(except image input)
 * @param xmls Path of IE model file(xml)
 * @param rawDetectionResults Outputs of network, they are raw data.
 * @param remoteSeverInfo parameters to do inference on remote server
 * @return Status code of infernce
 */
int vino_ie_pipeline_infer_image(std::vector<std::shared_ptr<cv::Mat>>& image,
                                 std::vector<std::vector<float>>& additionalInput,
                                 std::string xmls,
                                 std::vector<std::vector<float>*>& rawDetectionResults,
                                 struct serverParams& remoteServerInfo);

/**
 * @brief Do inference for image. For network with more than one image input.
 * @param images Image inputs for network. Each inner vector per input.
 * @param additionalInput Other inputs of network(except image input)
 * @param xmls Path of IE model file(xml)
 * @param rawDetectionResults Outputs of network, they are raw data.
 * @param remoteSeverInfo parameters to do inference on remote server
 * @return Status code of infernce
 */

int vino_ie_pipeline_infer_image(std::vector<std::vector<ptrCVImage>>& images,
                                 std::vector<std::vector<float>>& additionalInput,
                                 std::string xmls,
                                 std::vector<std::vector<float>*>& rawDetectionResults,
                                 struct serverParams& remoteServerInfo);

/**
 * @brief Do inference for speech (ASR). Using intel speech libraries.
 * @param samples Speech data buffer.
 * @param sampleLength Buffer size of speech data
 * @param bytesPerSample Size for each speech sample data (how many bytes for each sample)
 * @param rh_utterance_transcription Text result of speech. (ASR result)
 * @param remoteSeverInfo parameters to do inference on remote server.
 * @return Status code of infernce
 */
int vino_ie_pipeline_infer_speech(const short* samples,
                                  int sampleLength,
                                  int bytesPerSample,
                                  std::string config_path,
                                  std::vector<char> &rh_utterance_transcription,
                                  struct serverParams& remoteServerInfo);

/**
 * @brief Continously do inference for speech (ASR). Using intel speech libraries.
 * @param mode Working status of ASR. Start/inference/stop
 * @param samples Speech data buffer.
 * @param sampleLength Buffer size of speech data
 * @param bytesPerSample Size for each speech sample data (how many bytes for each sample)
 * @param rh_utterance_transcription Text result of speech. (ASR result)
 * @param config_path The file path for configuration file.
 * @param device The inference device.
 * @return Status code of infernce
 */
int vino_ie_pipeline_live_asr(int mode,  // 1 -- start  2 -- inference 0 -- stop
                              const short* samples,
                              int sampleLength,
                              int bytesPerSample,  // =2 if 16Kbits
                              std::string config_path,
                              std::string device,
                              std::vector<char> &rh_utterance_transcription);
/**
 * @brief Do inference for common models
 * @param inputData Data input for network. The data type is float.
 * @param additionalInput Other inputs of network(except image input)
 * @param xmls Path of IE model file(xml)
 * @param rawDetectionResults Outputs of network, they are raw data.
 * @param remoteSeverInfo parameters to do inference on remote server
 * @return Status code of infernce
 */
int vino_ie_pipeline_infer_common(std::vector<std::shared_ptr<std::vector<float>>>& inputData,
                                 std::vector<std::vector<float>>& additionalInput,
                                 std::string xmls,
                                 std::vector<std::vector<float>*>& rawDetectionResults,
                                 struct serverParams& remoteServerInfo);

/**
 * @brief Interface data structure between simulation IE lib and real IE container
 *     simulation IE lib  <<----- mock data interface ------>> fcgi/gRPC + real IE (OV container)
 * It includes input/output information of model.
 */
using mock_InputsDataMap = std::map<std::string, std::vector<unsigned long>>; // <name, getDims()>
using mock_OutputsDataMap = std::map<std::string, std::vector<unsigned long>>; // <name, getDims()>

struct mock_data {
    mock_InputsDataMap  inputBlobsInfo;
    mock_OutputsDataMap outputBlobsInfo;
    uint8_t layout; // NCHW = 1, NHWC = 2 ... must do mapping between uint8_t and enum type
};

struct modelParams {
    std::string modelXmlFile;
    bool needReshape; //true: reshape; false: no need reshape
    double aspectRatio; // valide only for needReshape=true
    int stride;         // valide only for need Reshape=true
};

/**
 * @brief Initialization before video inference
 * @param modelXmlFile Path of IE model file(xml)
 * @param deviceName Inference on which devic: CPU, GPU or others
 * @return Status code of infernce
 */
int vino_ie_video_infer_init(struct modelParams& model,
                             const std::string& deviceName,
			     struct mock_data& IOInformation);

/**
 * @brief Do inference for video frame
 * @param frame Image frame input for network
 * @param additionalInput Other inputs of network(except image input)
 * @param modelXmlFile Path of IE model file(xml)
 * @param rawDetectionResults Outputs of network, they are raw data.
 * @return Status code of infernce
 */
int vino_ie_video_infer_frame(const cv::Mat& frame,
                              std::vector<std::vector<float>>& additionalInput,
                              const std::string& modelXmlFile,
                              std::vector<std::vector<float>*>& rawDetectionResults);


/**
 * @brief Initialization before doing simulate IE inference
 * @param modelXmlFile Path of IE model file(xml)
 * @param batch Batch size used in this model
 * @param deviceName Inference on which devic: CPU, GPU or others
 * @param gnaScaler Scaler factor used in GNA devices
 * @param IOInformation IO information of the model. Returned to the caller
 * @return Status code of infernce
 */
int vino_simulate_ie_init(const std::string& modelXmlFile,
                          uint32_t& batch, //default=1
                          const std::string& deviceName,
                          uint32_t gnaScaler,
                          struct mock_data& IOInformation);

/**
 * @brief Do inference for simulate IE
 * @param audioData The speech data ready to do inference
 * @param modelXmlFile Path of IE model file(xml)
 * @param rawScoreResults Outputs of network, they are raw data.
 * @return Status code of infernce
 */
int vino_simulate_ie_infer(const std::string& audioData,
                           const std::string& modelXmlFile,
                           std::string& rawScoreResults);

/**
 * @brief Initial, load model from buffer
 * @param xmls a unique string to handle the inference entity
 * @param model model buffer
 * @param weights weights buffer
 * @param batch batch size
 * @param isImgInput whether input of model is image
 * @return Status code
 */
int vino_ie_pipeline_init_from_buffer(std::string xmls,
                                      const std::string &model,
                                      const std::string &weights,
                                      int batch,
                                      bool isImgInput);

/**
 * @brief Run inference from image
 * @param tensorData Buffers for input/output tensors
 * @param modelFile The model file, include path
 * @param backendEngine Specify the inferece engine, OPENVINO,ONNXRT,PYTORCH or TENSORFLOW.
 * @param remoteSeverInfo Parameters to do inference on remote server
 * @return Status code of infernce
 */
enum irtStatusCode irt_infer_from_image(struct irtImageIOBuffers& tensorData,
                                        const std::string& modelFile,
                                        std::string backendEngine,
                                        struct serverParams& remoteServerInfo);

/**
 * @brief Run inference from speech(ASR)
 * @param waveData Parameters for speech data, includes data buffer and settings.
 * @param configurationFile The configuration file, includes path
 * @param inferenceResult Text result of speech. (ASR result)
 * @param backendEngine Specify the inferece engine, OPENVINO,ONNXRT,PYTORCH or TENSORFLOW.
 * @param remoteSeverInfo Parameters to do inference on remote server
 * @return Status code of infernce
 */
enum irtStatusCode irt_infer_from_speech(const struct irtWaveData& waveData,
                                         std::string configurationFile,
                                         std::vector<char>& inferenceResult,
                                         std::string backendEngine,
                                         struct serverParams& remoteServerInfo);

/**
 * @brief Run inference from common model
 * @param tensorData Buffers for input/output tensors
 * @param modelFile The model file, include path
 * @param backendEngine Specify the inferece engine, OPENVINO,ONNXRT,PYTORCH or TENSORFLOW.
 * @param remoteSeverInfo Parameters to do inference on remote server
 * @return Status code of infernce
 */
enum irtStatusCode irt_infer_from_common(struct irtFloatIOBuffers& tensorData,
                                         const std::string& modelFile,
                                         std::string backendEngine,
                                         struct serverParams& remoteServerInfo);

