/* Copyright (C) 2021 Intel Corporation */

#include <ccai_stream_plugin.h>
#include <ccai_stream_utils.h>

#include <string.h>
#include <ctype.h>
#include <gst/gst.h>
#include <json-c/json.h>

#define D(fmt, ...) CS_D("[launcher]: " fmt, ##__VA_ARGS__);
#define I(fmt, ...) CS_I("[launcher]: " fmt, ##__VA_ARGS__);
#define E(fmt, ...) CS_E("[launcher]: " fmt, ##__VA_ARGS__);

#define CONFIG_FILE_1 "plugins/general_launcher.conf"
#define CONFIG_FILE_2 "/usr/lib/ccai_stream/plugins/general_launcher.conf"

struct desc_private {
	GstElement *gst_pipe;
	pthread_t tid;
	const char *pipeline_desc;
	const char *pipeline_desc_fmt;
};

static  char *new_gst_pipeline_desc(const char *fmt,
				    const char *source,
				    const char *sink,
				    const char *inference_interval,
				    const char *resolution)
{
	int len = snprintf(NULL, 0, fmt, source, sink, inference_interval, resolution) + 1;
	char *buf = malloc(len);
	if (buf == NULL)
		return NULL;

	sprintf(buf, fmt, source, sink, inference_interval, resolution);

	return buf;
}

static int generate_gst_pipeline_desc(struct ccai_stream_pipeline_desc *desc,
				     const char *param)
{
	if (param == NULL)
		return -1;

	struct desc_private *private = desc->private_data;
	if (private == NULL)
		return -1;

	D("pipeline param=%s\n", param);
	/* TODO: check pipe_name to do parse different params */

	struct json_object *obj = NULL;
	int ret = -1;
	char source[PATH_MAX];
	char sink[PATH_MAX];
	char resolution[PATH_MAX];
	char inference_interval[PATH_MAX];
	const char *p;
	const char *psrc = NULL;
	const char *psnk = NULL;
	const char *pres = NULL;
	const char *pinterval = NULL;

	if ((obj = json_tokener_parse(param)) == NULL) {
		E("fail to parse json.\n");
		goto error;
	}

	json_object_object_foreach(obj, key, value) {
		if (strncmp(key, "source", 6) == 0) {
			p = json_object_get_string(value);
			if (strncmp(p, "uri=", 4) == 0) {
				snprintf(source, sizeof(source),
					 "urisourcebin %s", p);
			} else if (strncmp(p, "device=", 7) == 0) {
				snprintf(source, sizeof(source),
					 "v4l2src %s", p);
			} else {
				snprintf(source, sizeof(source), "%s", p);
			}
			psrc = source;
		} else if (strncmp(key, "sink", 4) == 0) {
			p = json_object_get_string(value);
			snprintf(sink, sizeof(sink), "%s", p);
			psnk = sink;
		} else if (strncmp(key, "resolution", 10) == 0) {
			p = json_object_get_string(value);
			snprintf(resolution, sizeof(resolution),
				 "videoscale name=scale "
				 "! video/x-raw,format=BGR,%s !", p);
			pres = resolution;
		} else if (strncmp(key, "framerate", 9) == 0) {
			p = json_object_get_string(value);
			if (strncmp(p, "inference-interval=", 19) == 0) {
			        snprintf(inference_interval, sizeof(inference_interval),
					 "%s", p);
			} else {
			        snprintf(inference_interval, sizeof(inference_interval),
					 "%s", p);
			}
			pinterval = inference_interval;
		}
	}

	if (psrc == NULL || psnk == NULL)
		goto error;

	if (pres == NULL)
		snprintf(resolution, sizeof(resolution), "%s", " ");
	if (pinterval == NULL)
		snprintf(inference_interval, sizeof(inference_interval), "%s", " ");

	if ((private->pipeline_desc =
		new_gst_pipeline_desc(private->pipeline_desc_fmt,
				      source, resolution, inference_interval, sink)) == NULL)
		goto error;
	D("gst pipeline_desc=%s\n", private->pipeline_desc);
	ret = 0;
error:
	if (obj)
		json_object_put(obj);

	return ret;
}

static int create_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	D("desc->name=%s\n", desc->name);
	struct desc_private *private = desc->private_data;
	if (private == NULL)
		return -1;

	D("pipeline_desc_fmt: %s\n", private->pipeline_desc_fmt);
	if (generate_gst_pipeline_desc(desc, user_data) != 0)
		return -1;

	private->gst_pipe = gst_parse_launch(private->pipeline_desc, NULL);
	D("gst_pipe=%p\n", private->gst_pipe);
	if (private->gst_pipe == NULL)
		return -1;

	return 0;
}

static int start_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	if (desc == NULL)
		return -1;
	if (desc->private_data == NULL)
		return -1;

	struct desc_private *private = desc->private_data;

	ccai_gst_start_pipeline(private->gst_pipe);

	return 0;
}

static int stop_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	if (desc == NULL)
		return -1;
	if (desc->private_data == NULL)
		return -1;

	struct desc_private *private = desc->private_data;
	if (private->gst_pipe == NULL)
		return -1;

	ccai_gst_stop_pipeline(private->gst_pipe);
	return 0;
}

static int remove_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data)
{
	if (desc == NULL)
		return -1;
	if (desc->private_data == NULL)
		return -1;

	struct desc_private *private = desc->private_data;
	if (private->gst_pipe) {
		gst_object_unref(private->gst_pipe);
		private->gst_pipe = NULL;
	}

	return 0;
}

static GstElement* get_gst_pipeline(struct ccai_stream_pipeline_desc *desc)
{
	if (desc == NULL)
		return NULL;
	if (desc->private_data == NULL)
		return NULL;

	struct desc_private *private = desc->private_data;

	return private->gst_pipe;
}

static struct ccai_stream_pipeline_desc * new_pipeline_desc(const char *name,
							    const char *str)
{
	char *pipe_name = NULL;
	char *pipe_desc_fmt = NULL;
	struct ccai_stream_pipeline_desc *pipe = NULL;
	struct desc_private *private = NULL;

	if ((pipe_name = strndup(name, PATH_MAX)) == NULL)
		goto error;
	if ((pipe_desc_fmt = strndup(str, PATH_MAX)) == NULL)
		goto error;
	if ((pipe = g_try_new0(struct ccai_stream_pipeline_desc, 1)) == NULL)
		goto error;
	if ((private = g_try_new0(struct desc_private, 1)) == NULL)
		goto error;

	private->pipeline_desc_fmt = pipe_desc_fmt;

	pipe->name = pipe_name;
	pipe->create = create_pipe;
	pipe->start = start_pipe;
	pipe->stop = stop_pipe;
	pipe->remove = remove_pipe;
	pipe->read = NULL;
	pipe->config = NULL;
	pipe->get_gst_pipeline = get_gst_pipeline;
	pipe->private_data = private;

	return pipe;

error:
	if (pipe_name) free(pipe_name);
	if (pipe_desc_fmt) free(pipe_desc_fmt);
	if (pipe) free(pipe);
	if (private) free(private);

	return NULL;
}

static char *trim_whitespace(char *str)
{
	char *end;

	while(isspace((unsigned char)*str)) str ++;
	if(*str == 0)
		return str;
	end = str + strlen(str) - 1;
	while(end > str && isspace((unsigned char)*end)) end --;
	end[1] = '\0';

	return str;
}

static int parse_line(const char *data, char *key, size_t key_buflen,
		      char *value, size_t value_buflen)
{
	char fmt[64];
	if (key_buflen == 0 || value_buflen == 0)
		return 0;

	snprintf(fmt, sizeof(fmt), "%%%ds %%%d[^\n]s", (int)(key_buflen-1),
		 (int)(value_buflen-1));

	return sscanf(data, fmt, key, value);
}

static int load_conf()
{
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	FILE *fp = NULL;
	struct ccai_stream_pipeline_desc *desc = NULL;

	if ((fp = fopen(CONFIG_FILE_1, "r")) == NULL) {
		I("Cannot find configuration file: %s\n", CONFIG_FILE_1);
		if ((fp = fopen(CONFIG_FILE_2, "r")) == NULL) {
			E("Cannot find configuration files\n");
			return -1;
		}
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		D("Retrieved line of length %zu:\n", read);
		D("config line: %s", line);

		char *config = trim_whitespace(line);
		if (!strlen(config))
			continue;
		if (config[0] == '#')
			continue;

		char name[PATH_MAX];
		char str[PATH_MAX];

		if (parse_line(config, name, sizeof(name), str,
			       sizeof(str)) != 2) {
			E("config error: %s\n", line);
			continue;
		}
		D("name: %s, string: %s\n", name, str);

		if ((desc = new_pipeline_desc(name, str)) == NULL)
			continue;
		ccai_stream_add_pipeline(desc);
	}

	free(line);
	fclose(fp);

	return 0;
}

static int launcher_init()
{
	D("init\n");

	return load_conf();
}

static void launcher_exit()
{
	D("exit\n");
}

CCAI_STREAM_PLUGIN_DEFINE(launcher, "1.0",
			  CCAI_STREAM_PLUGIN_PRIORITY_DEFAULT,
			  launcher_init, launcher_exit)

