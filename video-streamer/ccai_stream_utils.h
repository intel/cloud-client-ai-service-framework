/* Copyright (C) 2021 Intel Corporation */

#ifndef CCAI_STREAM_UTILS_H
#define CCAI_STREAM_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ccai_stream.h>
#include <stdio.h>
#include <gst/gst.h>
#include <syslog.h>

extern int __ccai_stream_debug;
extern int __ccai_stream_log_target;

#define CCAI_STREAM_LOG_TARGET_SYSLOG	1
#define CCAI_STREAM_LOG_TARGET_STD	2

#define CS_D(fmt, ...) do { if (__ccai_stream_debug) { \
	if (__ccai_stream_log_target == CCAI_STREAM_LOG_TARGET_SYSLOG) \
		syslog(LOG_DEBUG, "[D][CS]:%s: " fmt, __func__, ##__VA_ARGS__); \
	else \
		fprintf(stdout, "[D][CS]:%s: " fmt, __func__, ##__VA_ARGS__); \
	} \
} while (0)
#define CS_I(fmt, ...) do { \
	if (__ccai_stream_log_target == CCAI_STREAM_LOG_TARGET_SYSLOG) \
		syslog(LOG_INFO, "[I][CS]:%s: " fmt, __func__, ##__VA_ARGS__); \
	else \
		fprintf(stdout, "[I][CS]:%s: " fmt, __func__, ##__VA_ARGS__); \
} while (0)
#define CS_E(fmt, ...) do { \
	if (__ccai_stream_log_target == CCAI_STREAM_LOG_TARGET_SYSLOG) \
		syslog(LOG_ERR, "[E][CS]:%s: " fmt, __func__, ##__VA_ARGS__); \
	else \
		fprintf(stderr, "[E][CS]:%s: " fmt, __func__, ##__VA_ARGS__); \
} while (0)

struct ccai_stream_pipeline_desc {
	const char *name;
	int (*create)(struct ccai_stream_pipeline_desc *, void *);
	int (*start)(struct ccai_stream_pipeline_desc *, void *);
	int (*stop)(struct ccai_stream_pipeline_desc *, void *);
	int (*remove)(struct ccai_stream_pipeline_desc *, void *);
	int (*read)(struct ccai_stream_pipeline_desc *, void *);
	int (*config)(struct ccai_stream_pipeline_desc *, void *);
	GstElement *(*get_gst_pipeline)(struct ccai_stream_pipeline_desc *);
	void *private_data;
};

int ccai_stream_add_pipeline(struct ccai_stream_pipeline_desc *pipe);
GstElement *ccai_stream_get_gst_pipeline(struct ccai_stream_pipeline *pipe);

void ccai_gst_start_pipeline(GstElement *gst_pipe);
void ccai_gst_stop_pipeline(GstElement *gst_pipe);

int ccai_gst_start_pipeline_thread(GstElement *gst_pipe, pthread_t *tid);
int ccai_gst_stop_pipeline_thread(GstElement *gst_pipe, pthread_t tid);

struct ccai_stream_probe_desc {
	const char *pipe_name;
	const char *name;
	int (*create)(struct ccai_stream_pipeline *,
		      struct ccai_stream_probe_desc *,
		      void *);
	void *private_data;
};
int ccai_stream_add_probe(struct ccai_stream_probe_desc *probe);
int ccai_stream_pipeline_add_probe_callback(struct ccai_stream_pipeline *pipe,
					    const char *element_name,
					    const char *pad_name,
					    GstPadProbeCallback callback,
					    void *user_data);

int ccai_stream_gst_pad_get_size(GstPad *pad, int *w, int *h);
const char* ccai_stream_gst_pad_get_format(GstPad *pad);

typedef int (*read_func)(struct ccai_stream_pipeline_desc *, void *);
int ccai_stream_set_read_func(struct ccai_stream_pipeline *pipe,
		              read_func read);

#ifdef __cplusplus
}
#endif
#endif
