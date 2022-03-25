/* Copyright (C) 2021 Intel Corporation */

#ifndef CCAI_STREAM_PLUGIN_H
#define CCAI_STREAM_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#define CCAI_STREAM_PLUGIN_PRIORITY_LOW		-100
#define CCAI_STREAM_PLUGIN_PRIORITY_DEFAULT	0
#define CCAI_STREAM_PLUGIN_PRIORITY_HIGHT	100

#define CCAI_STREAM_PLUGIN_VERSION	"1.0"

struct ccai_stream_plugin_desc {
	const char *name;
	const char *version;
	int priority;
	int (*init) (void);
	void (*exit) (void);
};

#define CCAI_STREAM_PLUGIN_DEFINE(name, version, priority, init, exit) \
	extern struct ccai_stream_plugin_desc ccai_stream_plugin_desc \
	__attribute__ ((visibility("default"))); \
	struct ccai_stream_plugin_desc ccai_stream_plugin_desc = { \
		#name, version, priority, init, exit \
	};


#ifdef __cplusplus
}
#endif
#endif
