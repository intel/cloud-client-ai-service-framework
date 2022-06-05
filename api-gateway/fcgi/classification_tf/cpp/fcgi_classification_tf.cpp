// Copyright (C) 2020 Intel Corporation

// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi
#include <string.h>
#include <sys/time.h>

#include <fstream>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

#include <fcgiapp.h>
#include <fcgio.h>

#include "vino_ie_pipe.hpp"
#include <ccai_log.h>
#include "fcgi_utils.h"


#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

std::string classification(cv::Mat img, std::string params_str) {
    CCAI_NOTICE("tensorflow classification c++ service");

    std::string model_file = "./models/inception_v3_2016_08_28_frozen.pb";
    std::string labelFileName = "./models/imagenet_slim_labels.txt";
    std::vector<std::vector<float>> additionalInput;
    std::vector<float> rawDetectionResult;
    std::vector<std::vector<float>*> rawDetectionResults;
    std::vector<std::string> labels;
    rawDetectionResults.push_back(&rawDetectionResult);

    cv::Size size(299, 299);
    cv::resize(img, img, size);
    img.convertTo(img, CV_32FC3, 1.0 / 255.0);

    std::vector<std::shared_ptr<cv::Mat>> images;
    std::shared_ptr<cv::Mat> frame_ptr = std::make_shared<cv::Mat>(img);
    std::ifstream inputFile;
    std::string ie_result = "{\n";
    images.push_back(frame_ptr);
    struct serverParams urlInfo {"https://api.ai.qq.com/fcgi-bin/image/image_tag", params_str};

    std::vector<std::vector<std::shared_ptr<cv::Mat>>> images_;
    images_.push_back(images);

    struct irtImageIOBuffers tensorData {&images_, &additionalInput, &rawDetectionResults};
    int res = irt_infer_from_image(tensorData, model_file, "TENSORFLOW", urlInfo);

    if (res == 0){
        //--------------------------get the result locally --------------------------------
        CCAI_NOTICE("local inference");
        ie_result += "\t\"ret\":0,\n";
        ie_result += "\t\"msg\":\"ok\",\n";

        ie_result += "\t\"data\":{\n";
        ie_result += "\t\t\"tag_list\":[\n";

        std::vector<float> &detection = rawDetectionResult;
        std::vector<float>::iterator biggest = std::max_element(std::begin(detection), std::end(detection));
        int position = std::distance(std::begin(detection), biggest);

        inputFile.open(labelFileName, std::ios::in);
        if (inputFile.is_open()) {
            std::string strLine;
            while (std::getline(inputFile, strLine)) {
                labels.push_back(strLine);
            }
        }

        ie_result += std::string("\t\t\t{\"tag_name\":\"") + labels[position] + "\",";
        ie_result += std::string("\"tag_confidence\":") + std::to_string(*biggest) + "}\n";
        ie_result += "\t\t]\n\t},\n";
    } else {
        ie_result += "\t\"ret\":1,\n";
        ie_result += "\t\"msg\":\"inference error\",\n";
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
        if ((pContent == NULL) || (strstr(pContent, "application/x-www-form-urlencoded") == 0)) {
            CCAI_NOTICE("get content error");
            std::string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
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
            std::string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "get content length error";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }

        std::string ie_result = "";
        std::string s_base64;
        cv::Mat img;
        post_data = (char *)malloc(data_length);
        if (post_data == NULL) {
            CCAI_NOTICE("malloc buffer error");
            std::string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
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
            ie_result = classification(img, post_str);
            //-----------------------------prepare output -----------------------------
            gettimeofday(&stop, NULL);
            double stop_time = stop.tv_sec + stop.tv_usec/1000000.0;
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
