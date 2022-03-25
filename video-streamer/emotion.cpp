// Copyright (C) 2021 Intel Corporation

#include <ccai_stream_plugin.h>
#include <ccai_stream_utils.h>
#include <ccai_stream.h>

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <opencv2/opencv.hpp>

#include "vino_ie_pipe.hpp"

#define D(fmt, ...) CS_D("[emotion]: " fmt, ##__VA_ARGS__);
#define I(fmt, ...) CS_I("[emotion]: " fmt, ##__VA_ARGS__);
#define E(fmt, ...) CS_E("[emotion]: " fmt, ##__VA_ARGS__);

#define MODEL_PATH "/opt/fcgi/cgi-bin/models/emotions-recognition-retail-0003.xml"


static const char* emotions_txt[] = {
	"neutral", "happy", "sad", "surprise", "anger"
};

static GstPadProbeReturn emotion_callback(GstPad *pad, GstPadProbeInfo *info,
					  gpointer user_data)
{
	D("emotion_callback\n");
	gsize bufsize;
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
	buffer = gst_buffer_make_writable(buffer);
	if (buffer == NULL) {
		E("cannot set buffer writable\n");
		return GST_PAD_PROBE_OK;
	}
	bufsize = gst_buffer_get_size(buffer);
	D("bufsize=%lu\n", bufsize);

	int w, h;
	const char *format;
	if (ccai_stream_gst_pad_get_size(pad, &w, &h) != 0) {
		E("cannot get pad size\n");
		return GST_PAD_PROBE_OK;
	}
	if ((format = ccai_stream_gst_pad_get_format(pad)) == NULL) {
		E("cannot get pad format\n");
		return GST_PAD_PROBE_OK;
	}
	D("w=%d, h=%d, format=%s\n", w, h, format);

	GstMapInfo map;
	if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
		E("failed to buffer map.");
		return GST_PAD_PROBE_OK;
	}
	// new a OpenCV Mat
	cv::Mat img(h, w, CV_8UC3, map.data);

	GstMeta *meta = NULL;
	gpointer state = NULL;
	GstVideoRegionOfInterestMeta *r;

	std::vector<std::vector<float>> additional;
	std::vector<std::vector<float>*> results;

	while ((meta = gst_buffer_iterate_meta_filtered(buffer, &state,
			GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))) {
		r = (GstVideoRegionOfInterestMeta *)meta;
		D("regi x=%u, y=%u, w=%u, h=%u\n", r->x, r->y, r->w, r->h);
		cv::Rect rect(r->x, r->y, r->w, r->h);
		cv::Mat face = img(rect);
		cv::Mat face_input(64, 64, CV_8UC3);
		cv::resize(face, face_input, face.size());

		cv::rectangle(img, rect, cv::Scalar(255, 0, 0));

		std::vector<float> res;
		additional.clear();
		results.clear();
		results.push_back(&res);
		res.clear();
		int ret = vino_ie_video_infer_frame(face_input, additional,
						    MODEL_PATH, results);
		if (ret == RT_INFER_ERROR) {
			E("failed to infer model=%s\n", MODEL_PATH);
			continue;
		}

		auto max_pos = max_element(res.begin(), res.end());
		int index = max_pos - res.begin();
		D("index=%d\n", index);
		if (index < 0 ||
		    (size_t)index >= sizeof(emotions_txt)/sizeof(char*)) {
			E("infer result error, index=%d\n", index);
			continue;
		}
		cv::putText(img, emotions_txt[index], cv::Point(10, 30),
			    cv::FONT_HERSHEY_DUPLEX, 1.0,
			    CV_RGB(118, 185, 0), 2);
	}

	gst_buffer_unmap(buffer, &map);

	return GST_PAD_PROBE_OK;
}

static int create(struct ccai_stream_pipeline *pipe,
		  struct ccai_stream_probe_desc *desc, void *user_data)
{
	D("create\n");
	if (ccai_stream_pipeline_add_probe_callback(pipe, "detect", "src",
						    emotion_callback,
						    desc) != 0)
		return -1;

        struct mock_data model_IOInfo;
        struct modelParams model{MODEL_PATH, false, 0, 0};
        vino_ie_video_infer_init(model, "CPU", model_IOInfo);

	return 0;
}

static struct ccai_stream_probe_desc desc = {
	"launcher.emotion2", "emotion", create, NULL
};

static int emot_init()
{
	D("init emotion\n");
	if (ccai_stream_add_probe(&desc) != 0) {
		E("failed to add probe\n");
		return -1;
	}

	return 0;
}

static void emot_exit()
{
	D("exit\n");
}

CCAI_STREAM_PLUGIN_DEFINE(emotion, "1.0",
			  CCAI_STREAM_PLUGIN_PRIORITY_DEFAULT - 10,
			  emot_init, emot_exit)

