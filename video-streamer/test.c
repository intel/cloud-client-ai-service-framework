/* Copyright (C) 2021 Intel Corporation */

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#include "ccai_stream.h"
#include "ccai_stream_utils.h"

#define D(fmt, ...) CS_D("[test]: " fmt, ##__VA_ARGS__);
#define I(fmt, ...) CS_I("[test]: " fmt, ##__VA_ARGS__);
#define E(fmt, ...) CS_E("[test]: " fmt, ##__VA_ARGS__);

#define FACE_DETECTION		0x01
#define OBJECT_DETECTION	0x02
#define BACKGROUND_BLUR		0x04
#define CAMERA_VIEW		0x08
#define EMOTION			0x10
#define FACE2			0x20
#define POSE                    0x30

#define FACE_DETECTION_PARAM "{ \
\"source\":\"uri=file:///home/wx/work/cfoc/human_pose_det/input/head-pose-face-detection-male.mp4\",\
\"sink\":\"gvametaconvert format=json ! gvametapublish method=file file-path=/home/wx/work/tmp/face_det_out.json ! fakesink sync=false\" \
}"

#define POSE_ESTIMATION_PARAM "{ \
\"source\":\"uri=file:///home/wx/work/cfoc/human_pose_det/input/male_head_center.jpg\",\
\"sink\":\"identity signal-handoffs=false ! fakesink sync=false\", \
\"framerate\":\"inference-interval=1\" \
}"

#define OBJECT_DETECTION_PARAM "{ \
\"source\":\"uri=file:///home/lu/disk/tmp/tmp/person-bicycle-car-detection.mp4\",\
\"sink\":\"fpsdisplaysink video-sink=ximagesink sync=false\",\
\"resolution\":\"width=800,height=450\" \
}"

#if 1
#define BACKGROUND_BLUR_PARAM "{ \
\"source\":\"uri=file:///home/lu/videos/head-pose-face-detection-female-and-male.mp4\",\
\"sink\":\"v4l2sink device=/dev/video2\",\
}"
#endif
#if 0
#define BACKGROUND_BLUR_PARAM "{ \
\"source\":\"uri=file:///home/lu/videos/head-pose-face-detection-female-and-male.mp4\",\
\"sink\":\"fpsdisplaysink video-sink=ximagesink sync=false\",\
}"
#endif
#if 0
#define BACKGROUND_BLUR_PARAM "{ \
\"source\":\"device=/dev/video0\",\
\"sink\":\"v4l2sink device=/dev/video2\",\
\"resolution\":\"width=800,height=600\" \
}"
#endif

#define CAMERA_VIEW_PARAM "{ \
\"source\":\"device=/dev/video2\",\
\"sink\":\"fpsdisplaysink video-sink=ximagesink sync=true\",\
}"

#if 0
#define EMOTION_PARAM "{ \
\"source\":\"uri=file:///home/lu/disk/tmp/tmp/head-pose-face-detection-female-and-male.mp4\",\
\"sink\":\"fpsdisplaysink video-sink=ximagesink sync=false\",\
\"resolution\":\"width=800,height=450\" \
}"
#else
#define EMOTION_PARAM "{ \
\"source\":\"device=/dev/video0\",\
\"sink\":\"fpsdisplaysink video-sink=ximagesink sync=false\",\
\"resolution\":\"width=800,height=450\" \
}"
#endif


#define MAX_PIPE_CNT 32

static void *start_pipeline(unsigned int flags, unsigned int mask,
			    const char *name, const char *param)
{
	void *p = NULL;

	if (flags & mask) {
		p = ccai_stream_pipeline_create(name, (void *)param);
		I("%s: %p\n", name, p);
		ccai_stream_pipeline_start(p, NULL);
	}

	return p;
}

static void stop_pipeline(void *pipe)
{
	ccai_stream_pipeline_stop(pipe, NULL);
	ccai_stream_pipeline_remove(pipe, NULL);
}

int main(int argc, char **argv)
{
	void *pipes[MAX_PIPE_CNT] = {};
	int cnt = 0, i;
	unsigned int start_pipelines = 0xFFFFFFFF;

	if (argc >= 2)
		start_pipelines = strtol(argv[1], NULL, 0);
	D("start_pipelines=0x%X\n", start_pipelines);

	ccai_stream_init();

	struct timeval start;
	gettimeofday(&start, NULL);
	printf("start tv_sec:%ld \n", start.tv_sec);
	printf("start tv_usec:%ld \n", start.tv_usec);

#if 0

	pipes[cnt++] = start_pipeline(start_pipelines, FACE_DETECTION,
				      "launcher.face_detection",
				      FACE_DETECTION_PARAM);
	pipes[cnt++] = start_pipeline(start_pipelines, POSE,
				      "launcher.pose_estimation",
				      POSE_ESTIMATION_PARAM);
	pipes[cnt++] = start_pipeline(start_pipelines, OBJECT_DETECTION,
				      "launcher.object_detection",
				      OBJECT_DETECTION_PARAM);
	pipes[cnt++] = start_pipeline(start_pipelines, BACKGROUND_BLUR,
				      "backgroundblur", BACKGROUND_BLUR_PARAM);
	pipes[cnt++] = start_pipeline(start_pipelines, EMOTION,
				      "launcher.emotion", EMOTION_PARAM);
	pipes[cnt++] = start_pipeline(start_pipelines, FACE2,
				      "launcher.face2", EMOTION_PARAM);
	for (i = 0; i < 5; i ++) {
		D(".\n");
		sleep(1);
	}

#endif
	void *bb;
	bb = ccai_stream_pipeline_create("backgroundblur",
					 BACKGROUND_BLUR_PARAM);

	// config
	struct ccai_stream_background_blur_config bc;
	bc.blur_type = CCAI_STREAM_BLUR_TYPE_IMAGE;
	snprintf(bc.image_file, sizeof(bc.image_file), "%s", "bg.jpeg");
	D("ccai_stream_pipeline_config bb=%p, bc.image_file=%s\n",
	  bb, bc.image_file);
	ccai_stream_pipeline_config(bb, &bc);

	// start
	ccai_stream_pipeline_start(bb, NULL);

	pipes[cnt++] = bb;

	for (i = 0; i < 2; i ++) { D(".\n"); sleep(1); }
	/* start camera view */
	pipes[cnt++] = start_pipeline(start_pipelines, CAMERA_VIEW,
				      "launcher.view", CAMERA_VIEW_PARAM);

	for (i = 0; i < 3; i ++) { D(".\n"); sleep(1); }

	/*bc.blur_type = CCAI_STREAM_BLUR_TYPE_BLUR;*/
	/*D("ccai_stream_pipeline_config bb=%p, bc.image_file=%s\n",*/
	  /*bb, bc.image_file);*/
	/*ccai_stream_pipeline_config(bb, &bc);*/

	for (i = 0; i < 15; i ++) { D(".\n"); sleep(1); }

	I("stop pipelines\n");
	for (i = 0; i < cnt; i++) {
		if (pipes[i])
			stop_pipeline(pipes[i]);
	}

	return 0;
}
