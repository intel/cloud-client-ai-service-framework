/* Copyright (C) 2021 Intel Corporation */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gst/gst.h>

#include "ccai_stream.h"
#include "ccai_stream_utils.h"
#include "cs.h"

int __ccai_stream_debug = 0;
int __ccai_stream_log_target = CCAI_STREAM_LOG_TARGET_SYSLOG;

static struct ccai_stream ccai_stream;

int ccai_stream_init()
{
	char *log_target = getenv("CCAI_STREAM_LOG_TARGET");
	if (log_target != NULL) {
		if (strncmp(log_target, "syslog", 6) == 0)
			__ccai_stream_log_target = CCAI_STREAM_LOG_TARGET_SYSLOG;
		else
			__ccai_stream_log_target = CCAI_STREAM_LOG_TARGET_STD;
	}

	char *debug = getenv("CCAI_STREAM_DEBUG");
	if (debug != NULL) {
		__ccai_stream_debug = atoi(debug);
	}

	gst_init(NULL, NULL);
	memset(&ccai_stream, 0, sizeof(ccai_stream));
	plugin_init();

	return 0;
}

struct ccai_stream_pipeline *
ccai_stream_pipeline_create(const char* pipeline_name, void *user_data)
{
	GSList *l, *ll;
	struct ccai_stream_pipeline *pipe = NULL;
	CS_D("pipeline_name=%s\n", pipeline_name);

	for (l = ccai_stream.pipelines; l; l = l->next) {
		struct ccai_stream_pipeline *p = l->data;
		struct ccai_stream_pipeline_desc *desc = p->desc;
		if (strcmp(desc->name, pipeline_name) != 0)
			continue;
		if (desc->create(desc, user_data) != 0)
			continue;
		for (ll = p->probes; ll; ll = ll->next) {
			struct ccai_stream_probe_desc *probe = ll->data;
			if (probe->create)
				probe->create(p, probe, user_data);
		}

		pipe = p;
		break;
	}

	return pipe;
}

int ccai_stream_pipeline_start(struct ccai_stream_pipeline *pipe,
			       void *user_data)
{
	if (pipe == NULL || pipe->desc == NULL || pipe->desc->start == NULL)
		return -1;

	return pipe->desc->start(pipe->desc, user_data);
}

int ccai_stream_add_pipeline(struct ccai_stream_pipeline_desc *desc)
{
	CS_D("add pipeline %s\n", desc->name);

	struct ccai_stream_pipeline *pipe;

	if ((pipe = g_try_new0(struct ccai_stream_pipeline, 1)) == NULL)
		return -1;

	pipe->desc = desc;

	ccai_stream.pipelines = g_slist_append(ccai_stream.pipelines,
					       (void*)pipe);

	return 0;
}

void ccai_gst_start_pipeline(GstElement *gst_pipe)
{
	GstStateChangeReturn ret =
		gst_element_set_state(gst_pipe, GST_STATE_PLAYING);
	CS_D("gst_element_set_state GST_STATE_PLAYING ret=%d\n", ret);
	/* TODO: should check gst element state */
}

void ccai_gst_stop_pipeline(GstElement *gst_pipe)
{
	GstStateChangeReturn ret =
		gst_element_set_state(gst_pipe, GST_STATE_NULL);
	CS_D("gst_element_set_state GST_STATE_NULL ret=%d\n", ret);
	/* TODO: should check pipeline state */
}

static void *gst_pipeline_proc(void* x)
{
	GstBus *bus;
	GstMessage *msg;

	GstElement *gst_pipe = (GstElement*)x;

	bus = gst_element_get_bus(gst_pipe);
	for (;;) {
		msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
						 GST_MESSAGE_ERROR |
						 GST_MESSAGE_EOS);

		if (msg == NULL)
			continue;

		const GstStructure *gst_struct;
		CS_D("message type: %d\n", GST_MESSAGE_TYPE(msg));
		gst_struct = gst_message_get_structure(msg);
		gchar *s = gst_structure_to_string(gst_struct);
		CS_E("error message: %s\n", s);
		g_free(s);
		gst_message_unref(msg);
		break;
	}
	gst_object_unref(bus);

	return NULL;
}

int ccai_gst_start_pipeline_thread(GstElement *gst_pipe, pthread_t *tid)
{
	GstStateChangeReturn ret;
	pthread_t id;

	if (gst_pipe == NULL)
		return -1;

	ret = gst_element_set_state(gst_pipe, GST_STATE_PLAYING);
	CS_D("gst_element_set_state GST_STATE_PLAYING ret=%d\n", ret);
	/* TODO: check pipeline state */
	pthread_create(&id, NULL, &gst_pipeline_proc, gst_pipe);

	if (tid)
		*tid = id;

	return 0;
}

int ccai_gst_stop_pipeline_thread(GstElement *gst_pipe, pthread_t tid)
{
	GstStateChangeReturn ret;

	ret = gst_element_set_state(gst_pipe, GST_STATE_NULL);
	CS_D("gst_element_set_state GST_STATE_NULL ret=%d\n", ret);
	/* TODO: check pipeline state */
	/* TODO: wait thread exit */
	/*pthread_join(tid, NULL);*/
	CS_D("pthread_join return\n");

	return 0;
}

int ccai_stream_pipeline_stop(struct ccai_stream_pipeline *pipe,
			      void *user_data)
{
	if (pipe == NULL || pipe->desc == NULL || pipe->desc->stop == NULL)
		return -1;

	return pipe->desc->stop(pipe->desc, user_data);
}

int ccai_stream_pipeline_remove(struct ccai_stream_pipeline *pipe,
				void *user_data)
{
	if (pipe == NULL || pipe->desc == NULL || pipe->desc->remove == NULL)
		return -1;

	return pipe->desc->remove(pipe->desc, user_data);
}

int ccai_stream_pipeline_read(struct ccai_stream_pipeline *pipe,
                              void *user_data)
{
	if (pipe == NULL || pipe->desc == NULL || pipe->desc->read == NULL)
		return -1;

	return pipe->desc->read(pipe->desc, user_data);
}

GstElement *ccai_stream_get_gst_pipeline(struct ccai_stream_pipeline *pipe)
{
	if (pipe == NULL || pipe->desc == NULL
	    || pipe->desc->get_gst_pipeline == NULL)
		return NULL;

	return pipe->desc->get_gst_pipeline(pipe->desc);
}

int ccai_stream_add_probe(struct ccai_stream_probe_desc *probe)
{
	GSList *list;

	for (list = ccai_stream.pipelines; list; list = list->next) {
		struct ccai_stream_pipeline *p = list->data;
		struct ccai_stream_pipeline_desc *desc = p->desc;
		if (strcmp(desc->name, probe->pipe_name) == 0) {
			p->probes = g_slist_append(p->probes, probe);
			return 0;
		}
	}

	return -1;
}

int ccai_stream_pipeline_add_probe_callback(struct ccai_stream_pipeline *pipe,
					    const char *element_name,
					    const char *pad_name,
					    GstPadProbeCallback callback,
					    void *user_data)
{
	GstElement *gst_pipe = NULL;
	GstElement *src = NULL;
	GstPad *pad = NULL;

	// gst pipeline
	if ((gst_pipe = ccai_stream_get_gst_pipeline(pipe)) == NULL) {
		CS_E("cannot get gst pipeline\n");
		goto error;
	}
	// element
	if ((src = gst_bin_get_by_name(GST_BIN(gst_pipe),
				       element_name)) == NULL) {
		CS_E("cannot get format_src\n");
		goto error;
	}
	// pad
	if ((pad = gst_element_get_static_pad(src, pad_name)) == NULL) {
		CS_E("cannot get pad\n");
		goto error;
	}

	// probe
	// TODO: save probe id to remove
	gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, callback, user_data,
			  NULL);

	gst_object_unref(pad);
	gst_object_unref(src);

	return 0;
error:
	return -1;
}

int ccai_stream_gst_pad_get_size(GstPad *pad, int *w, int *h)
{
	int ww, hh;
	GstCaps *caps = NULL;
	GstStructure *str;
	int ret = -1;

	if ((caps = gst_pad_get_current_caps(pad)) == NULL) {
		CS_E("current caps NULL\n");
		goto error;
	}
	if ((str = gst_caps_get_structure(caps, 0)) == NULL) {
		CS_E("cannot get structure\n");
		goto error;
	}
	if ((!gst_structure_get_int(str, "width", &ww)) ||
	    (!gst_structure_get_int(str, "height", &hh))) {
		CS_E("cannot get width,height\n");
		goto error;
	}

	if (w) *w = ww;
	if (h) *h = hh;

	ret = 0;

error:
	if (caps)
		gst_caps_unref(caps);
	return ret;
}

const char* ccai_stream_gst_pad_get_format(GstPad *pad)
{
	GstCaps *caps = NULL;
	GstStructure *str;
	const char *fmt = NULL;

	if ((caps = gst_pad_get_current_caps(pad)) == NULL) {
		CS_E("current caps NULL\n");
		goto error;
	}
	if ((str = gst_caps_get_structure(caps, 0)) == NULL) {
		CS_E("cannot get structure\n");
		goto error;
	}
	if ((fmt = gst_structure_get_string(str, "format")) == NULL)
		CS_E("cannot get format\n");

error:
	if (caps)
		gst_caps_unref(caps);

	return fmt;
}

int ccai_stream_set_read_func(struct ccai_stream_pipeline *pipe,
                              read_func read)
{
	if (pipe == NULL || pipe->desc == NULL)
		return -1;

        pipe->desc->read = read;

	return 0;
}

int ccai_stream_pipeline_config(struct ccai_stream_pipeline *pipe,
				void *user_data)
{
	if (pipe == NULL || pipe->desc == NULL || pipe->desc->config == NULL)
		return -1;

	return pipe->desc->config(pipe->desc, user_data);
}
