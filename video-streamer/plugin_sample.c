/* Copyright (C) 2021 Intel Corporation */
/*
   gcc `pkg-config --cflags gstreamer-1.0` -g -O2 \
	plugin_sample.c -o  sample.so \
	`pkg-config --libs gstreamer-1.0` \
	-shared -lccai_stream
 *
 */

#include <ccai_stream_plugin.h>
#include <ccai_stream_utils.h>
#include <gst/gst.h>

static const char *pipe_name = "sample";
static const char *gst_pipeline_desc = "videotestsrc ! ximagesink";

static int create_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	if (desc == NULL)
		return -1;
	desc->private_data = gst_parse_launch(gst_pipeline_desc, NULL);
	if (!desc->private_data)
		return -1;

	return 0;
}

static int start_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	if (desc == NULL || desc->private_data == NULL)
		return -1;
	GstElement *gst_pipe = (GstElement *)desc->private_data;

	ccai_gst_start_pipeline(gst_pipe);

	return 0;
}

static int stop_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	if (desc == NULL || desc->private_data == NULL)
		return -1;
	GstElement *gst_pipe = (GstElement *)desc->private_data;
	if (gst_pipe == NULL)
		return -1;

	ccai_gst_stop_pipeline(gst_pipe);

	return 0;
}

static int remove_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	if (desc == NULL || desc->private_data == NULL)
		return -1;
	GstElement *gst_pipe = (GstElement *)desc->private_data;

	if (gst_pipe) {
		gst_object_unref(gst_pipe);
		desc->private_data = NULL;
	}

	return 0;
}


static int sample_plugin_init()
{
	struct ccai_stream_pipeline_desc *desc;

	if ((desc = g_try_new0(struct ccai_stream_pipeline_desc, 1)) == NULL)
		return -1;

	desc->name = pipe_name;
	desc->create = create_pipe;
	desc->start = start_pipe;
	desc->stop = stop_pipe;
	desc->remove = remove_pipe;
	desc->config = NULL;
	desc->get_gst_pipeline = NULL;
	desc->private_data = NULL;

	ccai_stream_add_pipeline(desc);

	return 0;
}

static void sample_plugin_exit()
{
}

CCAI_STREAM_PLUGIN_DEFINE(sample, "1.0",
			  CCAI_STREAM_PLUGIN_PRIORITY_DEFAULT,
			  sample_plugin_init, sample_plugin_exit)
