// Copyright (C) 2020 Intel Corporation

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <fcgiapp.h>
#include <fcgio.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include <ccai_log.h>
#include <vino_ie_pipe.hpp>
#include "fcgi_utils.h"


#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0


struct Result {
    std::size_t label;
    float confidence;
    cv::Rect location;
};

std::string segmentation(cv::Mat img, std::string params_str) {
    CCAI_NOTICE("segmentation c++ service");

    std::string model_file = "./models/deeplabv3.xml";
    std::vector<std::vector<float>> additionalInput;
    std::vector<float> rawDetectionResult;
    std::vector<std::vector<float>*> rawDetectionResults;
    std::vector<std::string> labels;
    rawDetectionResults.push_back(&rawDetectionResult);

    std::vector<std::shared_ptr<cv::Mat>> images;
    std::shared_ptr<cv::Mat> frame_ptr = std::make_shared<cv::Mat>(img);
    cv::Mat frame = frame_ptr->clone();
    std::ifstream inputFile;
    std::string ie_result = "";
    if (frame.empty()) {
        ie_result = "error: frame is empty";
        return ie_result;
    }
    images.push_back(frame_ptr);
    struct serverParams urlInfo {"https://api.ai.qq.com/fcgi-bin/image/image_tag", params_str};
    int res = vino_ie_pipeline_infer_image(images, additionalInput, model_file,
                                           rawDetectionResults, urlInfo);
    int image_width = 513;
    int image_height = 513;

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

        std::vector<unsigned char> out_classes(image_height * image_width);

        for (int w = 0; w < image_width; w++) {
            for (int h = 0; h < image_height; h++) {
                for (size_t ch = 0; ch < 1; ch++){
                    out_classes[image_width * h + w] = static_cast<unsigned char>(detection[image_width * h + w]);
                }
            }
        }
        std::string out_str;
        out_str = EncodeBase(out_classes.data(), image_width * image_height);

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
            ie_result = ::segmentation(img, post_str);
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
