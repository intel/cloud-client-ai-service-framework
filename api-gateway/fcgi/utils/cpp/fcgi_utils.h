// Copyright (C) 2020 Intel Corporation
#pragma once

#include <stdlib.h>
#include <string>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include <vector>
//#include <memory>


//encode base64 data
std::string EncodeBase(const unsigned char* Data, int DataByte);

// decode base64 data
std::string DecodeBase(const char *base64_data, int size);

//convert base64 data to Mat image
cv::Mat Base2Mat(std::string &s_base64);

//convert base64 data to string
std::string Base2string(std::string &s_base64);

//convert char to short int
short int Char_dec(char data);

//decode %
std::string deData(const std::string &s_base64);

//get a part of characters
std::string get_data(std::string post_str, std::string name_str);
