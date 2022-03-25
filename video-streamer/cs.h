/* Copyright (C) 2021 Intel Corporation */

#ifndef CS_H
#define CS_H

#include <glib.h>
#include "ccai_stream_utils.h"

struct ccai_stream_pipeline {
	struct ccai_stream_pipeline_desc *desc;
	GSList *probes;
};

struct ccai_stream {
	GSList *pipelines;
};

int plugin_init();

#endif
