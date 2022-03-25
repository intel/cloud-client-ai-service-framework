// Copyright (C) 2020 Intel Corporation


// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi
#include <inference_engine.hpp>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <string>
#include <stdio.h>
#include<iostream>
#include<opencv2/highgui/highgui.hpp>
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

#define IE_RESULT_SIZE 21240//177*120
#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

using namespace InferenceEngine;
using namespace std;
using namespace cv;


struct Result {
    float accuracy;
    cv::Rect coordinate;
};
std::string facial_landmark(cv::Mat img, std::string params_str) {
    CCAI_NOTICE("facial landmark c++ service");

    std::string model_file = "./models/face-detection-adas-0001.xml";
    std::vector<std::vector<float>> additionalInput;
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

    struct serverParams urlInfo {"https://api.ai.qq.com/fcgi-bin/face/face_faceshape", params_str};

    int res = vino_ie_pipeline_infer_image(images, additionalInput, model_file,
                                           rawDetectionResults, urlInfo);
    if (res == RT_REMOTE_INFER_OK) {
        //------------------ get the result from  qq cloud --------------------------------
        CCAI_NOTICE("remote inference");

        int pos_start = urlInfo.response.find("{");
        int pos_end = (urlInfo.response.rfind("}"));
        ie_result = ie_result + urlInfo.response.substr(pos_start, pos_end - pos_start ) + ",\n";
    } else if (res == 0) {
        //--------------------------get the result locally --------------------------------

        CCAI_NOTICE("local inference");
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":0,\r\n";
        ie_result += "\t\"msg\":\"ok\",\r\n";
        ie_result += "\t\"data\":{\r\n";
        const int proposal_num = 200;
        const int item_size = 7;

        float threshold = 0.5;
        float image_width = static_cast<float>(frame.cols);
        float image_height = static_cast<float>(frame.rows);
        ie_result += "\t\t\"image_width\":";
        ie_result += std::to_string(image_width);
        ie_result += ",\n\t\t\"image_height\":";
        ie_result += std::to_string(image_height);
        ie_result += ",\n\t\t\"face_shape_list\":[\r\n";
        float enlarge_flag = 1.2;

        std::vector<Result> face_results;
        std::vector<int> base_x, base_y, base_w, base_h;

        for (int i = 0; i < proposal_num; i++) {

            float image_num = rawDetectionResult[0 + item_size * i];
            if (image_num < 0) {
                break;
            }

            Result face_result;
            face_result.accuracy = rawDetectionResult[2 + item_size * i];

            if (face_result.accuracy <= threshold) {
                continue;
            }

            face_result.coordinate.x = static_cast<int>(rawDetectionResult[3 + item_size * i] * image_width);
            face_result.coordinate.y = static_cast<int>(rawDetectionResult[4 + item_size * i] * image_height);
            face_result.coordinate.width = static_cast<int>(rawDetectionResult[5 + item_size * i] * image_width - face_result.coordinate.x);
            face_result.coordinate.height = static_cast<int>(rawDetectionResult[6 + item_size * i] * image_height - face_result.coordinate.y);

            // Make square and enlarge face bounding box for more robust operation of face analytics networks

            int point_x = face_result.coordinate.x + face_result.coordinate.width / 2;
            int point_y = face_result.coordinate.y + face_result.coordinate.height / 2;

            int face_size = std::max(face_result.coordinate.width, face_result.coordinate.height);

            int enlarge_width = static_cast<int>(enlarge_flag * face_size);
            int enlarge_height = static_cast<int>(enlarge_flag * face_size);

            face_result.coordinate.x = point_x - static_cast<int>(std::floor(enlarge_width / 2));
            face_result.coordinate.y = point_y - static_cast<int>(std::floor(enlarge_height / 2));

            face_result.coordinate.width = enlarge_width;
            face_result.coordinate.height = enlarge_height;

            if (face_result.accuracy > threshold) {
                face_results.push_back(face_result);
            }
        }

        if (face_results.empty()) {
            ie_result = "error: no face is detected";
            return ie_result;
        }

        //clear images vector prepare for next infer
        images.clear();
        for (auto &&face : face_results) {
            auto clipped_size = face.coordinate & cv::Rect(0, 0, image_width, image_height);
            std::shared_ptr<cv::Mat> clipped_image = std::make_shared<cv::Mat>(frame(clipped_size));
            images.push_back(clipped_image);
        }

        additionalInput.clear();
        rawDetectionResult.clear();

        // prepare for this new model, removing old data;
        std::string facial_landmark_model_file = "./models/facial-landmarks-35-adas-0002.xml";

        urlInfo.response.clear();
        res = vino_ie_pipeline_infer_image(images, additionalInput, facial_landmark_model_file,
                                           rawDetectionResults, urlInfo);

        int landmarks_size = 70;
        int flag = 0;

        for (size_t idx = 0; idx < face_results.size(); idx++) {
            auto begin = (landmarks_size * idx) /2;
            auto end = begin + landmarks_size / 2;
            for (auto i = begin; i < end; ++i) {
                float landmark_x = rawDetectionResult[2 * i];
                float landmark_y = rawDetectionResult[2 * i + 1];

                if (flag != 0) {
                    ie_result += ",\n";
                }

                flag++;
                ie_result = ie_result + "\t\t\t{\"x\":" + std::to_string(landmark_x * face_results[idx].coordinate.height + face_results[idx].coordinate.x) + ",\n";
                ie_result = ie_result + "\t\t\t\"y\":" + std::to_string(landmark_y * face_results[idx].coordinate.height + face_results[idx].coordinate.y) + "}";

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
        } else {
            sub_str = get_data(post_str, "image");
            s_base64 = deData(sub_str);
            img = Base2Mat(s_base64);

            //-----------------------------do inference and get the result -------------
            ie_result = facial_landmark(img, post_str);

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
