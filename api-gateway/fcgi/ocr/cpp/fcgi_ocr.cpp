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

void softmax(std::vector<float> *data) {
    auto &c_data = *data;
    const size_t dim_size = 2;

    for (size_t i = 0 ; i < c_data.size(); i += dim_size) {
        float fmax = std::max(c_data[i], c_data[i + 1]);
        c_data[i] = std::exp(c_data[i] - fmax);
        c_data[i + 1] = std::exp(c_data[i + 1] - fmax);
        float fsum = c_data[i] + c_data[i + 1];
        c_data[i] /= fsum;
        c_data[i + 1] /= fsum;
    }
}

std::vector<float> transpose(const std::vector<float>& input_data, const std::vector<size_t> &input_shape, const std::vector<size_t> &reshape_axes) {
    if (input_shape.size() != reshape_axes.size())
        throw std::runtime_error("Shape and axes must have the same dimension.");

    for (size_t axe : reshape_axes) {
        if (axe >= input_shape.size())
            throw std::runtime_error("Axis must be less than dimension of shape.");
    }

    size_t all_size = input_shape[0] * input_shape[1] * input_shape[2] * input_shape[3];
    size_t source_data_idx = 0;

    std::vector<float> output_data(all_size, 0);
    std::vector<size_t> idx(input_shape.size());
    std::vector<size_t> steps {input_shape[reshape_axes[1]] * input_shape[reshape_axes[2]] * input_shape[reshape_axes[3]],
                input_shape[reshape_axes[2]] * input_shape[reshape_axes[3]], input_shape[reshape_axes[3]], 1};

    for (idx[0] = 0; idx[0] < input_shape[0]; idx[0]++) {
        for (idx[1] = 0; idx[1] < input_shape[1]; idx[1]++) {
            for (idx[2] = 0; idx[2] < input_shape[2]; idx[2]++) {
                for (idx[3]= 0; idx[3] < input_shape[3]; idx[3]++) {
                    size_t output_data_idx = idx[reshape_axes[0]] * steps[0] + idx[reshape_axes[1]] * steps[1] +
                            idx[reshape_axes[2]] * steps[2] + idx[reshape_axes[3]] * steps[3];
                    output_data[output_data_idx] = input_data[source_data_idx++];
                }
            }
        }
    }
    return output_data;
}



std::vector<float> get_second_channel(const std::vector<float> &input_data) {
    std::vector<float> output_data(input_data.size() / 2, 0);
    for (size_t i = 0; i < input_data.size() / 2; i++) {
        output_data[i] = input_data[2 * i + 1];
    }
    return output_data;
}

std::vector<cv::RotatedRect> mask_to_rects(const cv::Mat &mask, float area_min, float height_min,
                                         cv::Size image_size) {
    double v_min;
    double v_max;

    std::vector<cv::RotatedRect> rects;

    cv::minMaxLoc(mask, &v_min, &v_max);
    cv::Mat mask_resized;
    cv::resize(mask, mask_resized, image_size, 0, 0, cv::INTER_NEAREST);

    int max_index = static_cast<int>(v_max);
    for (int i = 1; i <= max_index; i++) {

        cv::Mat mask_bbox = mask_resized == i;
        std::vector<std::vector<cv::Point>> mask_contours;

        cv::findContours(mask_bbox, mask_contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

        if (mask_contours.empty())
            continue;
        cv::RotatedRect rect = cv::minAreaRect(mask_contours[0]);
        if (std::min(rect.size.width, rect.size.height) < height_min)
            continue;
        if (rect.size.area() < area_min)
            continue;

        rects.emplace_back(rect);
    }

    return rects;
  }

int findRoot(int point, std::unordered_map<int, int> *group_mask) {
    int root = point;
    bool update_parent = false;
    while (group_mask->at(root) != -1) {
        root = group_mask->at(root);
        update_parent = true;
    }
    if (update_parent) {
        (*group_mask)[point] = root;
    }
    return root;
}

void join(int p1, int p2, std::unordered_map<int, int> *group_mask) {
    int root1 = findRoot(p1, group_mask);
    int root2 = findRoot(p2, group_mask);
    if (root1 != root2) {
        (*group_mask)[root1] = root2;
    }
}

cv::Mat all_masks(const std::vector<cv::Point> &ocr_points, int img_weight, int img_height,
                std::unordered_map<int, int> *image_mask) {

    std::unordered_map<int, int> roots_map;

    cv::Mat mask(img_height, img_weight, CV_32S, cv::Scalar(0));

    for (const auto &ocr_point : ocr_points) {
        int root_index = findRoot(ocr_point.x + ocr_point.y * img_weight, image_mask);
        if (roots_map.find(root_index) == roots_map.end()) {
            roots_map.emplace(root_index, static_cast<int>(roots_map.size() + 1));
        }
        mask.at<int>(ocr_point.x + ocr_point.y * img_weight) = roots_map[root_index];
    }

    return mask;
}

cv::Mat get_mask(const std::vector<float> &classification_data, const std::vector<int> & classification_data_shape,
                          const std::vector<float> &connection_data, const std::vector<int> & connection_data_shape,
                          float classification_conf_threshold, float connection_conf_threshold) {
    int img_height = classification_data_shape[1];
    int img_weight = classification_data_shape[2];

    std::vector<uchar> point_mask(img_height * img_weight, 0);
    std::vector<cv::Point> ocr_points;
    std::unordered_map<int, int> img_mask;


    for (size_t i = 0; i < point_mask.size(); i++) {
        point_mask[i] = classification_data[i] >= classification_conf_threshold;
        if (point_mask[i]) {
            ocr_points.emplace_back(i % img_weight, i / img_weight);
            img_mask[i] = -1;
        }
    }

    std::vector<uchar> connection_mask(connection_data.size(), 0);
    for (size_t i = 0; i < connection_mask.size(); i++) {
        connection_mask[i] = connection_data[i] >= connection_conf_threshold;
    }

    size_t near_points = size_t(connection_data_shape[3]);
    for (const auto &point : ocr_points) {
        size_t near_point = 0;
        for (int ny = point.y - 1; ny <= point.y + 1; ny++) {
            for (int nx = point.x - 1; nx <= point.x + 1; nx++) {
                if (nx == point.x && ny == point.y)
                    continue;
                if (nx >= 0 && nx < img_weight && ny >= 0 && ny < img_height) {
                    uchar point_value = point_mask[size_t(ny) * size_t(img_weight) + size_t(nx)];
                    uchar connection_value = connection_mask[
                        (size_t(point.y) * size_t(img_weight) + size_t(point.x)) * near_points + near_point];
                    if (point_value && connection_value) {
                        join(point.x + point.y * img_weight, nx + ny * img_weight, &img_mask);
                    }
                }
                near_point++;
            }
        }
    }

    return all_masks(ocr_points, img_weight, img_height, &img_mask);
}

std::vector<cv::RotatedRect> get_rects(std::vector<float> &connection_data, std::vector<float> &classification_data,  const cv::Size& image_size, float classification_threshold, float connection_threshold) {

    const int area_min = 300;
    const int height_min = 10;

    std::vector<size_t> connection_shape;
    connection_shape.emplace_back(1);
    connection_shape.emplace_back(16);
    connection_shape.emplace_back(192);
    connection_shape.emplace_back(320);
    connection_data = transpose(connection_data, connection_shape, {0, 2, 3, 1});

    softmax(&connection_data);
    connection_data = get_second_channel(connection_data);

    std::vector<size_t> classification_shape;
    classification_shape.emplace_back(1);
    classification_shape.emplace_back(2);
    classification_shape.emplace_back(192);
    classification_shape.emplace_back(320);
    classification_data = transpose(classification_data, classification_shape, {0, 2, 3, 1});
    softmax(&classification_data);
    classification_data = get_second_channel(classification_data);

    std::vector<int> new_connection_data_shape(4);
    new_connection_data_shape[0] = static_cast<int>(connection_shape[0]);
    new_connection_data_shape[1] = static_cast<int>(connection_shape[2]);
    new_connection_data_shape[2] = static_cast<int>(connection_shape[3]);
    new_connection_data_shape[3] = static_cast<int>(connection_shape[1]) / 2;

    std::vector<int> new_classification_data_shape(4);
    new_classification_data_shape[0] = static_cast<int>(classification_shape[0]);
    new_classification_data_shape[1] = static_cast<int>(classification_shape[2]);
    new_classification_data_shape[2] = static_cast<int>(classification_shape[3]);
    new_classification_data_shape[3] = static_cast<int>(classification_shape[1]) / 2;

    cv::Mat mask = get_mask(classification_data, new_classification_data_shape, connection_data, new_connection_data_shape, classification_threshold, connection_threshold);
    std::vector<cv::RotatedRect> rects = mask_to_rects(mask, static_cast<float>(area_min), static_cast<float>(height_min), image_size);

    return rects;
}



void softmax_(const std::vector<float>::const_iterator& begin, const std::vector<float>::const_iterator& end, int *arg_max, float *c_accuracy) {

    auto max_index = std::max_element(begin, end);
    *arg_max = static_cast<int>(std::distance(begin, max_index));

    float max_value = *max_index;
    double arg_sum = 0;

    for (auto i = begin; i != end; i++) {
        arg_sum += std::exp((*i) - max_value);
    }
    if (std::fabs(arg_sum) < std::numeric_limits<double>::epsilon()) {
        throw std::logic_error("sum can't be equal to zero");
    }

    *c_accuracy = 1.0f / static_cast<float>(arg_sum);
}


std::vector<cv::Point2f> rect_to_points(const cv::RotatedRect &rect) {
    cv::Point2f rect_vertices[4];
    std::vector<cv::Point2f> rect_points;

    rect.points(rect_vertices);

    for (int i = 0; i < 4; i++) {
        rect_points.emplace_back(rect_vertices[i].x, rect_vertices[i].y);
    }
    return rect_points;
}

cv::Point left_top_point(const std::vector<cv::Point2f> & points, int *idx) {
    cv::Point2f first_left(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    cv::Point2f second_left(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());

    int first_left_idx = -1;
    int second_left_idx = -1;

    for (size_t i = 0; i < points.size() ; i++) {
        if (first_left.x > points[i].x) {
            if (first_left.x < std::numeric_limits<float>::max()) {
                second_left = first_left;
                second_left_idx = first_left_idx;
            }
            first_left = points[i];
            first_left_idx = static_cast<int>(i);
        }
        if (second_left.x > points[i].x && points[i] != first_left) {
            second_left = points[i];
            second_left_idx = static_cast<int>(i);
        }
    }

    if (second_left.y < first_left.y) {
        first_left = second_left;
        first_left_idx = second_left_idx;
    }

    *idx = first_left_idx;
    return first_left;
}

cv::Mat crop_image(const cv::Mat &frame, const std::vector<cv::Point2f> &rect_points, const cv::Size& target_size, int top_left_index) {
    cv::Point2f first_point = rect_points[static_cast<size_t>(top_left_index)];
    cv::Point2f second_point = rect_points[(top_left_index + 1) % 4];
    cv::Point2f third_point = rect_points[(top_left_index + 2) % 4];
    cv::Mat crop_image(target_size, CV_8UC3, cv::Scalar(0));

    std::vector<cv::Point2f> from_point{first_point, second_point, third_point};
    std::vector<cv::Point2f> to_point{cv::Point2f(0.0f, 0.0f), cv::Point2f(static_cast<float>(target_size.width-1), 0.0f),
                                cv::Point2f(static_cast<float>(target_size.width-1), static_cast<float>(target_size.height-1))};

    cv::Mat affine_image = cv::getAffineTransform(from_point, to_point);

    cv::warpAffine(frame, crop_image, affine_image, crop_image.size());

    return crop_image;
}

std::string ocr(cv::Mat img, std::string params_str){
    CCAI_NOTICE("ocr c++ service");

    std::string detection_model_file = "./models/text-detection-0003.xml";
    std::string recognition_model_file = "./models/text-recognition-0012.xml";

    std::vector<std::vector<float>> additionalInput;
    std::vector<float> rawDetectionResult1, rawDetectionResult2;
    std::vector<std::vector<float>*> rawDetectionResults;

    rawDetectionResults.push_back(&rawDetectionResult1);
    rawDetectionResults.push_back(&rawDetectionResult2);

    std::vector<std::shared_ptr<cv::Mat>> detection_images;
    std::shared_ptr<cv::Mat> frame_ptr = std::make_shared<cv::Mat>(img);
    cv::Mat frame = frame_ptr->clone();

    std::string ie_result = "";
    if (frame.empty()) {
        ie_result = "error: frame is empty";
        return ie_result;
    }
    detection_images.push_back(frame_ptr);

    struct serverParams urlInfo{"https://api.ai.qq.com/fcgi-bin/ocr/ocr_generalocr", params_str};

    int res = vino_ie_pipeline_infer_image(detection_images, additionalInput, detection_model_file,
                                           rawDetectionResults, urlInfo);
    if (res == RT_REMOTE_INFER_OK) {
        //------------------ get the result from  qq cloud --------------------------------
        CCAI_NOTICE("remote inference");
        CCAI_NOTICE("%s", urlInfo.response.c_str());
        int pos_start = urlInfo.response.find("{");
        int pos_end = (urlInfo.response.substr(pos_start)).rfind("}");

        ie_result += urlInfo.response.substr(pos_start, pos_end);
        ie_result += ",\n";
    } else if (res == 0) {
        //--------------------------get the result locally --------------------------------
        CCAI_NOTICE("local inference");
        ie_result += "{\r\n";
        ie_result += "\t\"ret\":0,\r\n";
        ie_result += "\t\"msg\":\"ok\",\r\n";

        ie_result += "\t\"data\":{\r\n";
        ie_result += "\t\t\"item_list\":[\r\n";

        std::vector<cv::RotatedRect> rects;
        float classification_threshold = 0.8;
        float connection_threshold = 0.8;
        rects = get_rects(rawDetectionResult1, rawDetectionResult2, frame.size(), classification_threshold, connection_threshold);
        int flag_item = 1;
        for (const auto &rect : rects) {
            cv::Mat cropped_image;
            std::vector<cv::Point2f> rect_points;
            int top_left_index = 0;

            if (rect.size != cv::Size2f(0, 0)) {
                rect_points = rect_to_points(rect);

                left_top_point(rect_points, &top_left_index);
                if (flag_item != 1) {
                    ie_result += ",\n";
                }
                flag_item ++;
                ie_result += "\t\t{\n";
                ie_result += "\t\t\t\"itemcoord\":[\n";
                ie_result += "\t\t\t{\n";
                ie_result += "\t\t\t\t\"x\":" + std::to_string(rect_points[top_left_index].x) +",\n";
                ie_result += "\t\t\t\t\"y\":" + std::to_string(rect_points[top_left_index].y) +",\n";
                ie_result += "\t\t\t\t\"width\":" + std::to_string(rect.size.width) +",\n";
                ie_result += "\t\t\t\t\"height\":" + std::to_string(rect.size.height) +"\n";
                ie_result += "\t\t\t}\n";
                ie_result += "\t\t\t],\n";

                cv::Size target_size = cv::Size(120, 32);
                cropped_image = crop_image(frame, rect_points, target_size, top_left_index);

                cv::Mat grey_cropped_image;
                cv::cvtColor(cropped_image, grey_cropped_image, cv::COLOR_BGR2GRAY); 

                additionalInput.clear();
                rawDetectionResult1.clear();
                std::vector<std::shared_ptr<cv::Mat>> recognition_images;
                frame_ptr = std::make_shared<cv::Mat>(grey_cropped_image);
                recognition_images.push_back(frame_ptr);

                urlInfo.response.clear();
                res = vino_ie_pipeline_infer_image(recognition_images, additionalInput, recognition_model_file,
                                                   rawDetectionResults, urlInfo);

                std::string alphabet = "0123456789abcdefghijklmnopqrstuvwxyz#";
                std::string s_result = "";

                bool prev_flag = false;
                const int alphabet_length = alphabet.length();
                char stop_symbol = '#';
                double accuracy = 1.0;

                ie_result = ie_result + "\t\t\t\"words\":[\n";

                int flag_character = 1;
                for (std::vector<float>::const_iterator it = rawDetectionResult1.begin(); it != rawDetectionResult1.end(); it += alphabet_length) {
                    int arg_max;
                    float c_accuracy;

                    softmax_(it, it + alphabet_length, &arg_max, &c_accuracy);
                    (accuracy) *= c_accuracy;
                    auto symbol = alphabet[arg_max];
                    if (symbol != stop_symbol) {
                        if (s_result.empty() || prev_flag || (!s_result.empty() && symbol != s_result.back())) {
                            prev_flag = false;
                            s_result += symbol;
                            if (flag_character != 1) {
                                ie_result += ",\n";
                            }

                            flag_character ++;
                            ie_result = ie_result + "\t\t\t{\n";
                            ie_result = ie_result + "\t\t\t\t\"character\":\"" + symbol + "\",\n";
                            ie_result = ie_result + "\t\t\t\t\"confidence\":" + std::to_string(c_accuracy) + "\n";
                            ie_result = ie_result + "\t\t\t}";

                        }
                    } else {
                        prev_flag = true;
                    }
                }
                ie_result = ie_result + "\t\t\t],\n";
                ie_result = ie_result + "\t\t\t\"itemstring\":\"" + s_result + "\"\n";
                ie_result = ie_result + "\t\t}";

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

int main(int argc, char ** argv) {
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
        post_data [ret] = '\0';
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

            //-----------------------------do inference and get the result --------------
            ie_result = ocr(img, post_str);
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
