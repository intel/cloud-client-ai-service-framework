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

using namespace InferenceEngine;
using namespace std;
using namespace cv;


struct Result {
    std::size_t label;
    float confidence;
    cv::Rect location;
};


std::string read_file(const std::string& path) {
    auto ss = std::ostringstream{};
    std::ifstream file(path);
    ss << file.rdbuf();
    return ss.str();
}

void load_model()
{
    std::string model;
    std::string weights;
    std::string model_path = "./models/single-image-super-resolution-1032.xml";

    std::size_t pos = model_path.find(".xml");
    std::string weights_path = model_path.substr(0, pos) + ".bin";

    model = read_file(model_path);
    weights = read_file(weights_path);
    vino_ie_pipeline_init_from_buffer(model_path, model, weights, 1, true);
}

std::string super_resolution(cv::Mat img, std::string params_str) {
    CCAI_NOTICE("super_resolution c++ service");

    std::string model_file = "./models/single-image-super-resolution-1032.xml";

    std::vector<std::vector<float>> additionalInputs;
    std::vector<float> rawDetectionResult;
    std::vector<std::vector<float>*> rawDetectionResults;
    rawDetectionResults.push_back(&rawDetectionResult);

    std::vector<std::shared_ptr<cv::Mat>> images;
    std::shared_ptr<cv::Mat> frame_ptr = std::make_shared<cv::Mat>(img);

    cv::Mat frame = frame_ptr->clone();
    std::string ie_result = "";
    if (frame.empty()) {
        ie_result = "error: frame is empty";
        return ie_result;
    }
    images.push_back(frame_ptr);

    struct serverParams urlInfo {"https://api.ai.qq.com/fcgi-bin/image/image_tag", params_str};

    std::vector<std::vector<ptrCVImage>> images_vecs;
    images_vecs.push_back(images);
    images_vecs.push_back(images);
    images.clear();

    int res = vino_ie_pipeline_infer_image(images_vecs, additionalInputs, model_file,
                                                     rawDetectionResults, urlInfo);
    if (res == RT_REMOTE_INFER_OK) {
        //------------------ get the result from  qq cloud --------------------------------
        CCAI_NOTICE("remote inference");
        CCAI_NOTICE("%s", urlInfo.response.c_str());
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":2,\r\n";
        ie_result += "\t\"msg\":\"can not support remote inference\"";

    } else if (res == 0){
        //--------------------------get the result locally --------------------------------
        CCAI_NOTICE("local inference");
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":0,\r\n";
        ie_result += "\t\"msg\":\"ok\",\r\n";
        ie_result += "\t\"data\":\r\n";

        std::vector<float> &detection = rawDetectionResult;
        size_t h = 1080;
        size_t w = 1920;
        size_t numOfPixels = w * h;
        size_t numOfChannels = 3;
        std::vector<cv::Mat> imgPlanes;
        imgPlanes = std::vector<cv::Mat>{
            cv::Mat(h, w, CV_32FC1, &(detection[0])),
            cv::Mat(h, w, CV_32FC1, &(detection[numOfPixels])),
            cv::Mat(h, w, CV_32FC1, &(detection[numOfPixels * 2]))};

        for (auto & img : imgPlanes)
            img.convertTo(img, CV_8UC1, 255);
        cv::Mat resultImg;
        cv::merge(imgPlanes, resultImg);

        std::vector<unsigned char>img_vector(resultImg.reshape(1, numOfPixels * numOfChannels));
        std::string out_str = "";
        out_str = EncodeBase(img_vector.data(), numOfPixels * numOfChannels );

        ie_result +="\"";
        ie_result += out_str;
        ie_result +="\"";

    } else {
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":1,\r\n";
        ie_result += "\t\"msg\":\"inference error\"";
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
    load_model();
    while (1) {
        err = FCGX_Accept_r(&cgi);
        if (err) {
            std::string log_info = "FCGX_Accept_r stopped: " + std::to_string(err);
            CCAI_NOTICE("%s", log_info.c_str());
            break;
        }
        //-----------------------check content type------------------------------
        char *pContent = FCGX_GetParam("CONTENT_TYPE", cgi.envp);
        if ((pContent == NULL) || (strstr(pContent, "application/x-www-form-urlencoded") == 0)) {
            CCAI_NOTICE("get content error");
            string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "not Acceptable";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }
        //----------------------------get the post_data form fcgi-------------------
        struct timeval start, stop;
        gettimeofday(&start, NULL);
        double start_time = start.tv_sec + start.tv_usec/1000000.0;
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
            ie_result = super_resolution(img, post_str);
            //-----------------------------prepare output -----------------------------
            gettimeofday(&stop, NULL);
            double stop_time = stop.tv_sec + stop.tv_usec/1000000.0;
            result += ie_result;
            double time = stop_time - start_time + 0.02;
            std::string time_str = std::to_string(time);
            int pos_start = time_str.find(".");
            std::string cut_time_str = time_str.substr(0, pos_start) + time_str.substr(pos_start, 4);

            result += ",\n\t\"time\":" + cut_time_str + "\n}";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
        }
        //------------------free memory----------------------
        free(post_data);

    }

    return 0;
}
