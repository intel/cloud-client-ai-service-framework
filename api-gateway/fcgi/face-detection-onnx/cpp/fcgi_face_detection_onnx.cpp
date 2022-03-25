// Copyright (C) 2020 Intel Corporation


// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi
#include <inference_engine.hpp>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <string>
#include <stdio.h>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <fcgiapp.h>
#include <fcgio.h>
#include <fcgi_stdio.h>
#include <functional>
#include <random>
#include <memory>
#include <chrono>
#include <utility>
#include <algorithm>
#include <iterator>
#include <map>
#include <sstream>
#include <unistd.h>
#include <format_reader_ptr.h>
#include <samples/ocv_common.hpp>
#include "vino_ie_pipe.hpp"
#include <ccai_log.h>
#include <sys/time.h>
#include "fcgi_utils.h"

#ifdef WITH_EXTENSIONS
#include <ext_list.hpp>
#endif


#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

using namespace std;

static constexpr int64_t IMG_W = 640;
static constexpr int64_t IMG_H = 480;
static constexpr int64_t IMG_CHANNEL = 3;
static constexpr float CONFIDENCE_THRESHOLD = 0.7;
static constexpr float NMS_THRESHOLD = 0.3;

static void preprocessTranspose(float* dst, const unsigned char* src,
                                const int64_t width, const int64_t height,
                                const int channels) {
    for (int c = 0; c < channels; ++c) {
        for (int i = 0; i < height; ++i) {
            for (int j = 0; j < width; ++j) {
                dst[c * height * width + i * width + j] =
                    (src[i * width * channels + j * channels + c] - 127.) / 128.;
            }
        }
    }
}

static std::deque<size_t> sortIndexes(const std::vector<float>& v) {
    std::deque<size_t> indices(v.size());
    std::iota(indices.begin(), indices.end(), 0); //assign value begin from 0;

    std::stable_sort(std::begin(indices), std::end(indices), [&v](size_t i1, size_t i2) { return v[i1] < v[i2]; });

    return indices;
}

static std::vector<uint64_t> nms(const std::vector<std::array<float, 4>>& bboxes,
                                 const std::vector<float>& scores, const float overlapThresh = 0.45,
                                 const uint64_t topK = std::numeric_limits<uint64_t>::max()) {

    uint64_t boxesLength = bboxes.size();
    const uint64_t realK = std::max(std::min(boxesLength, topK), static_cast<uint64_t>(1));

    std::vector<uint64_t> keepIndices;
    keepIndices.reserve(realK);

    std::deque<uint64_t> sortedIndices = sortIndexes(scores);

    // keep only topk bboxes
    for (uint64_t i = 0; i < boxesLength - realK; ++i) {
        sortedIndices.pop_front();
    }

    std::vector<float> areas;
    areas.reserve(boxesLength);
    std::transform(std::begin(bboxes), std::end(bboxes), std::back_inserter(areas), //calculate area
                   [](const std::array<float, 4>& elem) { return (elem[2] - elem[0]) * (elem[3] - elem[1]); });

    while (!sortedIndices.empty()) {
        uint64_t currentIdx = sortedIndices.back();
        keepIndices.emplace_back(currentIdx);

        if (sortedIndices.size() == 1) {
            break;
        }

        sortedIndices.pop_back();
        std::vector<float> ious;
        ious.reserve(sortedIndices.size());

        const auto& curBbox = bboxes[currentIdx];
        const float curArea = areas[currentIdx];

        std::deque<uint64_t> newSortedIndices;

        for (const uint64_t elem : sortedIndices) {
            const auto& bbox = bboxes[elem];
            float tmpXmin = std::max(curBbox[0], bbox[0]);
            float tmpYmin = std::max(curBbox[1], bbox[1]);
            float tmpXmax = std::min(curBbox[2], bbox[2]);
            float tmpYmax = std::min(curBbox[3], bbox[3]);

            float tmpW = std::max<float>(tmpXmax - tmpXmin, 0.0);
            float tmpH = std::max<float>(tmpYmax - tmpYmin, 0.0);

            const float intersection = tmpW * tmpH;
            const float tmpArea = areas[elem];
            const float unionArea = tmpArea + curArea - intersection;
            const float iou = intersection / unionArea;

            if (iou <= overlapThresh) {
                newSortedIndices.emplace_back(elem);
            }
        }

        sortedIndices = newSortedIndices;
    }

    return keepIndices;
}

static std::string face_detection(cv::Mat inputImg, std::string params_str) {
    CCAI_NOTICE("face detection c++ service, onnx version");

    std::string modelFile = "./models/version-RFB-640.onnx";

    int origH = inputImg.rows;
    int origW = inputImg.cols;

    cv::Mat processedImg;
    cv::resize(inputImg, processedImg, cv::Size(IMG_W, IMG_H));

    cv::cvtColor(processedImg, processedImg, cv::COLOR_BGR2RGB);

    std::vector<float> dst(IMG_CHANNEL * IMG_H * IMG_W);
    preprocessTranspose(dst.data(), processedImg.data, IMG_W, IMG_H, 3);

    std::vector<ptrCVImage> batchImages;
    ptrCVImage frame_ptr = std::make_shared<cv::Mat>(dst);

    batchImages.push_back(frame_ptr);
    std::vector<std::vector<ptrCVImage>> images;
    images.push_back(batchImages);

    std::vector<std::vector<float>> additionalInput;

    std::vector<float> confidenceResult;
    std::vector<float> bboxResult;
    std::vector<std::vector<float>*> rawDetectionResults;
    rawDetectionResults.push_back(&confidenceResult);
    rawDetectionResults.push_back(&bboxResult);

    struct irtImageIOBuffers tensorData{&images, &additionalInput, &rawDetectionResults};

    std::string ie_result;
    struct serverParams urlInfo {"https://api.ai.qq.com/fcgi-bin/face/face_detectmultiface", params_str};
    int res = irt_infer_from_image(tensorData, modelFile, "ONNXRT", urlInfo);

    if (res == RT_REMOTE_INFER_OK) {

        //------------------ get the result from  qq cloud --------------------------------
        CCAI_NOTICE("remote inference");
        CCAI_NOTICE("%s", urlInfo.response.c_str());

        int pos_start = urlInfo.response.find("{");
        int pos_end = (urlInfo.response.substr(pos_start)).rfind("}");
        ie_result = ie_result + urlInfo.response.substr(pos_start, pos_end) + ",\n";
    } else if (res == 0){
        //--------------------------get the result locally --------------------------------

        CCAI_NOTICE("local inference");
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":0,\r\n";
        ie_result += "\t\"msg\":\"ok\",\r\n";

        ie_result += "\t\"data\":{\r\n";
        ie_result += "\t\t\"face_list\":[\r\n";


        int numAnchors = 17640;
        float confThresh = CONFIDENCE_THRESHOLD;
        int flag = 1;
        std::vector<std::array<float, 4>> bboxes;
        std::vector<float> scores;

        for (auto indices = 0; indices < numAnchors; indices++) {
            float conf = confidenceResult[indices * 2 + 1];
            if (conf < confThresh) {
                continue;
            }
            float xmin = bboxResult[indices * 4 + 0] * origW;
            float ymin = bboxResult[indices * 4 + 1] * origH;
            float xmax = bboxResult[indices * 4 + 2] * origW;
            float ymax = bboxResult[indices * 4 + 3] * origH;

            bboxes.emplace_back(std::array<float, 4>{xmin, ymin, xmax, ymax});
            scores.emplace_back(conf);
         }

         if (bboxes.size() != 0) {
            float nmsThresh = NMS_THRESHOLD;
            auto afterNmsIndices = nms(bboxes, scores, nmsThresh);

            for (const auto idx : afterNmsIndices) {
                if (flag != 1) {
                    ie_result += ",\n";
                }
                flag++;
                ie_result += "\t\t{\n";
                ie_result = ie_result + "\t\t\t\"x1\":" + std::to_string(bboxes[idx][0]); //xmin
                ie_result = ie_result + ",\n\t\t\t\"y1\":" + std::to_string(bboxes[idx][1]); //ymin
                ie_result = ie_result + ",\n\t\t\t\"x2\":" + std::to_string(bboxes[idx][2]); //xmax
                ie_result = ie_result + ",\n\t\t\t\"y2\":" + std::to_string(bboxes[idx][3]); //ymax
                ie_result += "\n\t\t}";
            }
        }

        ie_result += "\t\t]\r\n";
        ie_result += "\t},\r\n";
    } else {
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":1,\r\n";
        ie_result += "\t\"msg\":\"inference error\",\r\n";

        CCAI_NOTICE("can not get inference results");
    }

    return ie_result;
}


int main(int argc, char **argv) {
    int err = FCGX_Init(); /* call before Accept in multithreaded apps */
    if (err) {
        std::string log_info="FCGX_Init failed: " + std::to_string(err);
        CCAI_NOTICE("%s", log_info.c_str());
        return 1;
    }

    FCGX_Request cgi;
    err = FCGX_InitRequest(&cgi, LISTENSOCK_FILENO, LISTENSOCK_FLAGS);
    if (err) {
        std::string log_info = "FCGX_InitRequest failed: " + std::to_string(err);
        CCAI_NOTICE("%s", log_info.c_str());
        return 2;
    }

    while (1) {
        err = FCGX_Accept_r(&cgi);
        if (err) {
            std::string log_info = "FCGX_Accept_r stopped: " + std::to_string(err);
            CCAI_NOTICE("%s", log_info.c_str());
            break;
        }
        //-----------------------check content type------------------------------
        char *pContent = FCGX_GetParam("CONTENT_TYPE", cgi.envp);
        if ((pContent == NULL) || (strstr(pContent, "application/x-www-form-urlencoded") == NULL)) {
            CCAI_NOTICE("get content error");
            string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "not Acceptable";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }
        //----------------------------get the post_data form fcgi-------------------
        struct timeval start, stop;
        gettimeofday(&start, NULL);
        double start_time = start.tv_sec + start.tv_usec / 1000000.0;
        char *post_data = NULL;
        char *pLenstr = FCGX_GetParam("CONTENT_LENGTH", cgi.envp);
        unsigned long data_length = 1;

        if (pLenstr == NULL ||
            (data_length += strtoul(pLenstr, NULL, 10)) > INT32_MAX) {
            CCAI_NOTICE("get length error");
            string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "get content length error";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }

        std::string ie_result = "";
        string s_base64;
        cv::Mat img;
        post_data = (char *)malloc(data_length);

        if (post_data == NULL) {
            CCAI_NOTICE("malloc buffer error");
            string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "malloc buffer error";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }
        int ret = 0;
        ret = FCGX_GetStr(post_data, data_length, cgi.in);
        post_data[ret] = '\0';
        std::string post_str = post_data;

        std::string result("Status: 200 OK\r\nContent-Type: text/html\r\ncharset: utf-8\r\n"
                           "X-Content-Type-Options: nosniff\r\nX-frame-options: deny\r\n\r\n");

        std::string sub_str = "";
        sub_str = get_data(post_str, "healthcheck");
        if (sub_str == "1") {
            result = result + "healthcheck ok";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
        }
        else{
            sub_str = get_data(post_str, "image");
            s_base64 = deData(sub_str);
            img = Base2Mat(s_base64);

            //-----------------------------do inference and get the result ------------
            ie_result = face_detection(img, post_str);
            //-----------------------------prepare output -----------------------------
            gettimeofday(&stop, NULL);
            double stop_time = stop.tv_sec + stop.tv_usec / 1000000.0;

            result += ie_result;

            double time = stop_time - start_time;
            std::string time_str = std::to_string(time);
            int pos_start = time_str.find(".");
            std::string cut_time_str = time_str.substr(0, pos_start) + time_str.substr(pos_start, 4);
            result += "\t\"time\":" + cut_time_str + "\n}";

            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
        }
        //------------------free memory----------------------
        free(post_data);

    }

    return 0;
}
