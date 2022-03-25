/* Copyright (C) 2020 Intel Corporation */

#ifndef GRPC_INFERENCE_SERVICE_H
#define GRPC_INFERENCE_SERVICE_H

#include <opencv2/opencv.hpp>

#ifndef DEBUG
#define DEBUG 0
#endif

#define UNIFIED_LOG

#ifdef UNIFIED_LOG
#include <ccai_log.h>
#define debug_log(fmt, ...) do { if (DEBUG) \
		CCAI_DEBUG("AI-Service-Framework %s: " fmt, __func__,  ##__VA_ARGS__); \
	} while (0)
#define info_log(fmt, ...) \
	do { CCAI_INFO("AI-Service-Framework " fmt, ##__VA_ARGS__); } while (0)
#define error_log(fmt, ...) \
	do { CCAI_ERROR("AI-Service-Framework " fmt, ##__VA_ARGS__); } while (0)
#else
#define debug_log(fmt, ...) do { if (DEBUG) \
		fprintf(stdout, "%s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)
#define info_log(fmt, ...) do { fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)
#define error_log(fmt, ...) do { \
		fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)
#endif

int infer(cv::Mat img, const char *model, std::vector<std::vector<float>*> &r);

#endif
