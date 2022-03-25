/* Copyright (C) 2021 Intel Corporation */

#ifndef CCAI_STREAM_H
#define CCAI_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/limits.h>

//max support 3 human
#define MAX_PEOPLE  3

#define HEADER_ANGLE_AVAILABLE    (1 << 0)
#define SHOULDER_ANGLE_AVAILABLE  (1 << 1)
#define Y_P_R_AVAILABLE           (1 << 2)
#define LEFT_EYE_AVAILABLE        (1 << 3)
#define RIGHT_EYE_AVAILABLE       (1 << 4)
#define BODY_POSITION_AVAILABLE   (1 << 5)

struct ccai_stream_pose_result {
	int pose_number;
	int value_available[MAX_PEOPLE];
	int shoulder_angle[MAX_PEOPLE];
	int header_angle[MAX_PEOPLE];
	float y_angle[MAX_PEOPLE];
	float p_angle[MAX_PEOPLE];
	float r_angle[MAX_PEOPLE];
	int left_eye_state[MAX_PEOPLE];
	int right_eye_state[MAX_PEOPLE];
	int body_status[MAX_PEOPLE]; // 1=left screen, 0=ok, center, 2=right screen
};

#define CCAI_STREAM_BLUR_TYPE_BLUR		0
#define CCAI_STREAM_BLUR_TYPE_IMAGE		1

struct ccai_stream_background_blur_config {
	int blur_type;
	char image_file[PATH_MAX];
};

struct ccai_stream_pipeline;

int ccai_stream_init();
struct ccai_stream_pipeline *
	ccai_stream_pipeline_create(const char* pipeline_name,
				    void *user_data);
int ccai_stream_pipeline_start(struct ccai_stream_pipeline *pipe,
			       void *user_data);
int ccai_stream_pipeline_stop(struct ccai_stream_pipeline *pipe,
			      void *user_data);
int ccai_stream_pipeline_remove(struct ccai_stream_pipeline *pipe,
				void *user_data);
int ccai_stream_pipeline_read(struct ccai_stream_pipeline *pipe,
			      void *user_data);
int ccai_stream_pipeline_config(struct ccai_stream_pipeline *pipe,
				void *user_data);

#ifdef __cplusplus
}
#endif
#endif
