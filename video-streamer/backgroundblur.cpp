// Copyright (C) 2021 Intel Corporation

#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <mutex>

#include <ccai_stream_plugin.h>
#include <ccai_stream_utils.h>
#include <ccai_stream.h>

#include <gst/gst.h>
#include <opencv2/opencv.hpp>

#include "vino_ie_pipe.hpp"

#define D(fmt, ...) CS_D("[backgroundblur]: " fmt, ##__VA_ARGS__);
#define I(fmt, ...) CS_I("[backgroundblur]: " fmt, ##__VA_ARGS__);
#define E(fmt, ...) CS_E("[backgroundblur]: " fmt, ##__VA_ARGS__);

#define MODEL_PATH "/opt/fcgi/cgi-bin/models/deeplabv3.xml"

//#define print_timestamp(msg)
#define print_timestamp(msg) _print_timestamp(msg)

struct desc_private {
	struct ccai_stream_pipeline *pipe;
	int blur_type;
	cv::Mat bg_img;
	std::mutex mtx;
};

static void _print_timestamp(const char *msg)
{
	char buffer[30];
	struct timeval tv;
	time_t curtime;

	gettimeofday(&tv, NULL);
	curtime=tv.tv_sec;

	strftime(buffer, sizeof(buffer), "%m-%d %T.", localtime(&curtime));
	printf("[%s%06ld] %s\n", buffer, tv.tv_usec, msg);
}

static GstPadProbeReturn blur_callback(GstPad *pad, GstPadProbeInfo *info,
				       gpointer user_data)
{
	struct ccai_stream_pipeline_desc *desc =
		(struct ccai_stream_pipeline_desc *)user_data;
	struct desc_private *p = (struct desc_private *)desc->private_data;

	print_timestamp("blur_callback start");

	//D("blur_callback\n");
	// get image buffer
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
	buffer = gst_buffer_make_writable(buffer);
	if (buffer == NULL) {
		E("probe info buffer NULL\n");
		return GST_PAD_PROBE_OK;
	}

	//gsize bufsize;
	//bufsize = gst_buffer_get_size(buffer);
	//D("bufsize=%lu\n", bufsize);

	GstMapInfo map;
	if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
		E("Failed to buffer map.");
		return GST_PAD_PROBE_OK;
	}

	int w, h;
	if (ccai_stream_gst_pad_get_size(pad, &w, &h) != 0)
		return GST_PAD_PROBE_OK;
	//D("size=%dx%d\n", w, h);

	// new a cv::Mat to write image buffer
	cv::Mat orig_img(h, w, CV_8UC3, map.data);

	print_timestamp("infer start");
	// infer
	std::vector<float> result;
	std::vector<std::vector<float>> additional_input;
	std::vector<std::vector<float> *> rs;
	rs.push_back(&result);
	result.clear();
	vino_ie_video_infer_frame(orig_img, additional_input, MODEL_PATH, rs);
	print_timestamp("infer end");

	print_timestamp("build segmentation map");
	// get segmentation image
	cv::Mat orig_segmentation(513, 513, CV_8U);
	for (size_t hh = 0; hh < 513; hh++) {
		for (size_t ww = 0; ww < 513; ww++) {
			unsigned char val = result[hh * 513 + ww];
			if (val == 15) //15 is person
				orig_segmentation.at<uchar>(hh, ww) = true;
			else
				orig_segmentation.at<uchar>(hh, ww) = false;
		}
	}

	cv::Mat segmentation(orig_img.size(), CV_8U);
	cv::resize(orig_segmentation, segmentation, orig_img.size());
	print_timestamp("build segmentation map ok");
	print_timestamp("build background");

	cv::Mat tmp_img(h, w, CV_8UC3);

	p->mtx.lock();
	switch (p->blur_type) {
	case CCAI_STREAM_BLUR_TYPE_IMAGE:
		cv::resize(p->bg_img, tmp_img, tmp_img.size());
		if (p->bg_img.rows != h || p->bg_img.cols != w) {
			D("resize background image\n");
			p->bg_img = tmp_img.clone();
		}
		break;
	case CCAI_STREAM_BLUR_TYPE_BLUR:
	default:
		// blur orig_img to tmp_img
		cv::blur(orig_img, tmp_img, cv::Size(20, 20));
		break;
	}
	p->mtx.unlock();
	print_timestamp("build background ok");

	print_timestamp("write person to tmp_img");
	// write person segmentation image to tmp_img
	for (int hh = 0; hh < h; hh++) {
		for (int ww = 0; ww < w; ww++) {
			if (segmentation.at<uchar>(hh, ww)) {
				tmp_img.at<cv::Vec3b>(hh, ww)[0] =
					orig_img.at<cv::Vec3b>(hh, ww)[0];
				tmp_img.at<cv::Vec3b>(hh, ww)[1] =
					orig_img.at<cv::Vec3b>(hh, ww)[1];
				tmp_img.at<cv::Vec3b>(hh, ww)[2] =
					orig_img.at<cv::Vec3b>(hh, ww)[2];
			}
		}
	}
	print_timestamp("write person to tmp_img ok");
	// copy tmp_img back to orig_img
	print_timestamp("copy image to orig_img");
	cv::resize(tmp_img, orig_img, orig_img.size());
	print_timestamp("copy image to orig_img ok");

	gst_buffer_unmap(buffer, &map);
	GST_PAD_PROBE_INFO_DATA(info) = buffer;

	print_timestamp("blur_callback return");
	return GST_PAD_PROBE_OK;
}

static int create_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	struct ccai_stream_pipeline *pipe = NULL;
	struct desc_private *p = NULL;

	struct mock_data model_IOInfo;
	struct modelParams model{MODEL_PATH, false, 0, 0};

	if ((pipe = ccai_stream_pipeline_create("launcher.video", user_data))
	    == NULL) {
		E("create_pipe error\n");
		goto error;
	}
	if ((p = g_try_new0(struct desc_private, 1)) == NULL)
		goto error;

	if (ccai_stream_pipeline_add_probe_callback(pipe, "format_src",
						    "sink", blur_callback,
						    desc) != 0) {
		goto error;
	}

	vino_ie_video_infer_init(model, "CPU", model_IOInfo);

	p->pipe = pipe;
	desc->private_data = p;

	return 0;

error:
	if (pipe)
		ccai_stream_pipeline_remove(pipe, NULL);
	if (p)
		free(p);

	return -1;
}

static int start_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	struct desc_private *p = (struct desc_private *)desc->private_data;
	ccai_stream_pipeline_start(p->pipe, NULL);

	return 0;
}

static int stop_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	struct desc_private *p = (struct desc_private *)desc->private_data;
	ccai_stream_pipeline_stop(p->pipe, NULL);

	return 0;
}

static int remove_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	struct desc_private *p = (struct desc_private *)desc->private_data;
	ccai_stream_pipeline_remove(p->pipe, NULL);

	return 0;
}

static int config_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	struct desc_private *p = (struct desc_private *)desc->private_data;
	struct ccai_stream_background_blur_config *config =
		(struct ccai_stream_background_blur_config *)user_data;

	D("backgroundblur config_pipe p=%p\n", p);
	D("backgroundblur config type=%d\n", config->blur_type);
	D("backgroundblur config image_file=%s\n", config->image_file);

	cv::Mat img = cv::imread(config->image_file, cv::IMREAD_COLOR);
	if (img.data == NULL)
		return -1;

	p->mtx.lock();
	p->bg_img = img.clone();
	p->blur_type = config->blur_type;
	p->mtx.unlock();

	return 0;
}

struct ccai_stream_pipeline_desc desc = {
	"backgroundblur",
	create_pipe, start_pipe, stop_pipe, remove_pipe, NULL, config_pipe,
	NULL, NULL,
};

static int bb_init()
{
	D("init\n");

	ccai_stream_add_pipeline(&desc);

	return 0;
}

static void bb_exit()
{
	D("exit\n");
}

CCAI_STREAM_PLUGIN_DEFINE(backgroundblur, "1.0",
			  CCAI_STREAM_PLUGIN_PRIORITY_DEFAULT - 10,
			  bb_init, bb_exit)

