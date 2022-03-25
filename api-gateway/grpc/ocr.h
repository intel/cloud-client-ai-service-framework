// Copyright (C) 2020 Intel Corporation

#ifndef OCR_H
#define OCR_H

#include <string>
#include <iostream>
#include <fstream>
#include <memory>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>


std::string ocr(cv::Mat img);

#endif
