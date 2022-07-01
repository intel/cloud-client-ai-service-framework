// Copyright (C) 2020 Intel Corporation

/*
API example
=======
curl -d '{
"pipeline":"launcher.object_detection",
"method":"start",
"parameter":"{
\"source\":\"device=/dev/video0\",
\"sink\":\"v4l2sink device=/dev/video2\",
\"resolution\":\"width=800,height=600\"
}"
}' -H "Content-Type: application/json" -X POST http://localhost:8080/cgi-bin/streaming

gst-launch-1.0 -vvv v4l2src device=/dev/video2 ! decodebin ! videoconvert ! ximagesink

*/

#include "fcgi_utils.h"

#include <fcgiapp.h>
#include <fcgio.h>
#include <ccai_stream.h>
#include <json-c/json.h>

#include <string>
#include <stdio.h>
#include <sys/time.h>

#include <thread>
#include <mutex>
#include <chrono>

#include <ccai_log.h>
#include "limbs_status.hpp"

#define I(fmt, ...) do { \
	CCAI_NOTICE("[I]: " fmt, ##__VA_ARGS__); \
} while(0)

#define DEFAULT_PARAMETER \
	"{" \
	"\"source\":\"device=/dev/video0\"," \
	"\"sink\":\"v4l2sink device=/dev/video2\"," \
	"\"resolution\":\"width=800,height=600\"" \
	"}"

#define DELAY_TIME      300 //second 300s=5min

struct humans {
        struct human_limbs peple[MAX_PEOPLE];
	int numbers;
};

struct delayTimer {
       bool enableTimer; // true: started  false: end
       std::chrono::steady_clock::time_point startTime;
} gDelayTimer;

static struct humans gHumans;
static std::mutex gMtx;
static bool pooling = false;
static	std::thread thread_fun;

static std::map<std::string, ccai_stream_pipeline*> pipelines;

static int config_backgroundblur(ccai_stream_pipeline *p, const char *config)
{
	struct json_object *obj = NULL;
	const char *blur_type = NULL;
	const char *image_file = NULL;
	struct ccai_stream_background_blur_config bc;
	int ret = -1;

	if ((obj = json_tokener_parse(config)) == NULL) {
		I("json_tokener_parse error, config=%s\n", config);
		return ret;
	}
	json_object_object_foreach(obj, key, value) {
		if (strncmp(key, "blur_type", 9) == 0) {
			blur_type = json_object_get_string(value);
		} else if (strncmp(key, "image_file", 10) == 0) {
			image_file = json_object_get_string(value);
		}
	}
	if (strncmp(blur_type, "image", 5) == 0) {
		if (image_file == NULL)
			goto out;

		bc.blur_type = CCAI_STREAM_BLUR_TYPE_IMAGE;
		snprintf(bc.image_file, sizeof(bc.image_file), "%s",
			 image_file);
		I("ccai_stream_pipeline_config image_file=%s\n",
		  bc.image_file);
	} else {
		// blur
		bc.blur_type = CCAI_STREAM_BLUR_TYPE_BLUR;
		I("ccai_stream_pipeline_config blur_type=blur\n");
	}
	if (ccai_stream_pipeline_config(p, &bc) != 0)
		goto out;

	ret = 0;
out:
	if (obj)
		json_object_put(obj);

	return ret;
}

static int config_pipeline(const char *name, const char *config)
{
	ccai_stream_pipeline *p = NULL;
	std::map<std::string, ccai_stream_pipeline*>::iterator it;

	it = pipelines.find(name);
	if (it == pipelines.end()) {
		I("cannot find pipeline: %s, ignore method stop\n", name);
		return -1;
	}
	p = it->second;

	if (strncmp(name, "backgroundblur", 14) == 0 && config) {
		return config_backgroundblur(p, config);
	}

	return -1;
}


static int stop_pipeline(const char *name, const char *param)
{
	ccai_stream_pipeline *p = NULL;
	std::map<std::string, ccai_stream_pipeline*>::iterator it;

	it = pipelines.find(name);
	if (it == pipelines.end()) {
		I("cannot find pipeline: %s, ignore method stop\n", name);
		return 0;
	}

	p = it->second;
	if (ccai_stream_pipeline_stop(p, (void *)param) != 0)
		return -1;
	if (ccai_stream_pipeline_remove(p, (void *)param) != 0)
		return -1;

	pipelines.erase(it);

	return 0;
}

static int start_pipeline(const char *name, const char *param,
			  const char *config)
{
	ccai_stream_pipeline *p = NULL;
	std::map<std::string, ccai_stream_pipeline*>::iterator it;

	it = pipelines.find(name);
	if (it != pipelines.end()) {
		I("pipeline: %s, not change state, ignore method start\n",
		  name);
		return 0;
	}

	if ((p = ccai_stream_pipeline_create(name, (void *)param)) == NULL)
		return -1;

	I("%s: %p\n", name, p);

	if (ccai_stream_pipeline_start(p, (void *)param) != 0) {
		return -1;
	}

	if (strncmp(name, "backgroundblur", 14) == 0 && config) {
		config_backgroundblur(p, config);
	}

	pipelines[name] = p;

	return 0;
}

static std::map<int, std::string> yaw_result = {{0, ""}, {1, "Shake left "}, {2, "Shake right "}};
static std::map<int, std::string> pitch_result = {{0, ""}, {1, "Down "}, {2, "Up "}};
static std::map<int, std::string> roll_result = {{0, ""}, {1, "Tilt left "}, {2, "Tilt right "}};
static std::map<int, std::string> shoulder_result = {{-1, "-"}, {0, "OK"}, {1, "Slide left"}, {2, "Slide right"}};
static std::map<int, std::string> position_result = {{-1, "-"}, {0, "OK"}, {1, "Deviation left"}, {2, "Deviation right"}};

static int read_pipeline(const char *name, std::string& result)
{
        std::map<std::string, ccai_stream_pipeline*>::iterator it;

        it = pipelines.find(name);
        if (it == pipelines.end()) {
                I("cannot find pipeline: %s, ignore method read\n", name);
                return 0;
        }

        if (strncmp(name, "launcher.pose_estimation", 24) == 0) {
                struct humans tmpHumans;
		{
                    std::lock_guard<std::mutex> lck(gMtx);
                    tmpHumans = gHumans;
		}

                if (tmpHumans.numbers == 0) {
		    auto current_time = std::chrono::steady_clock::now();
                    if (!gDelayTimer.enableTimer) {
                        gDelayTimer.enableTimer = true;
			gDelayTimer.startTime = current_time;
		    } else if (std::chrono::duration_cast<std::chrono::seconds>(current_time - gDelayTimer.startTime).count() > DELAY_TIME) {
                        gDelayTimer.enableTimer = false;
			std::string message = "Can not detect face for more than 5 minutes, stop the pipeline to save power. Please restart it if you like!";
		        result += "\r\n{\r\n";
                        result += "\t\"Warnning\":\"" + message + "\"\r\n";
                        result += "}\r\n";

		        return -1;
		    }
		} else {
                    gDelayTimer.enableTimer = false;
		}

		result += "\r\n{\r\n";
                for (int i = 0; i < tmpHumans.numbers; i++) {
                    //convert to string
		    std::string eye_status = "-";
                    int left_eye = tmpHumans.peple[i].left_eye.get_status();
                    int right_eye = tmpHumans.peple[i].right_eye.get_status();
		    if ((left_eye == _EYE_OPEN_) && (right_eye == _EYE_OPEN_))
		        eye_status = "OK";
		    else if ((left_eye == _EYE_CLOSE_) && (right_eye == _EYE_CLOSE_))
			eye_status = "CLOSE";

                    std::string header_str;
		    int status = tmpHumans.peple[i].head.get_status();
                    if (status == 0) {
			header_str = "OK";
		    } else if (status == -1) {
			header_str = "-";
		    } else {
                        int y = (status >> _YAW_ANGLES_) & 0x0F;
                        int p = (status >> _PITCH_ANGLES_) & 0x0F;
                        int r = (status >> _ROLL_ANGLES_) & 0x0F;

			header_str = yaw_result[y] + pitch_result[p] + roll_result[r];
                    }

		    status = tmpHumans.peple[i].shoulder.get_status();
                    std::string shoulder_str = shoulder_result[status];

		    status = tmpHumans.peple[i].body_position.get_status();
                    std::string body_status = position_result[status];

		    std::string pose_result = tmpHumans.peple[i].is_corrected ? "OK" : "INCORRECT";

                    result += "\"Person" + std::to_string(i) + "\":{\r\n";
                    result += "\t\"body status\":\"" + pose_result + "\",\r\n";
                    result += "\t\"header status\":\"" + header_str + "\",\r\n";
                    result += "\t\"shoulder status\":\"" + shoulder_str + "\",\r\n";
                    result += "\t\"eye status\":\"" + eye_status + "\",\r\n";
                    result += "\t\"body position\":\"" + body_status + "\"\r\n";
                    if (i == (tmpHumans.numbers - 1))
                        result += "\t}\r\n";
                    else
                        result += "\t},\r\n";
                } // end for(i=)

                result += "}\r\n";
	}

        return 0;
}

static int update_status(struct ccai_stream_pose_result& pose_status) {

	for (int i = 0; i < pose_status.pose_number; i++) {
		int status = _LIMBS_INVALID_;
                if (pose_status.value_available[i] & LEFT_EYE_AVAILABLE)
                         status = pose_status.left_eye_state[i] == 1 ?  _EYE_OPEN_ : _EYE_CLOSE_;
		gHumans.peple[i].left_eye.update_status(status);

		status = _LIMBS_INVALID_;
                if (pose_status.value_available[i] & RIGHT_EYE_AVAILABLE)
                         status = pose_status.right_eye_state[i] == 1 ?  _EYE_OPEN_ : _EYE_CLOSE_;
		gHumans.peple[i].right_eye.update_status(status);

		status = _LIMBS_INVALID_;
                if (pose_status.value_available[i] & Y_P_R_AVAILABLE) {
		       status = _LIMBS_OK_;
                       if (std::abs(pose_status.y_angle[i]) > 20) { //30
                           status |= (pose_status.y_angle[i] > 0) ? (_LIMBS_LEFT_OR_DOWN_ << _YAW_ANGLES_) : (_LIMBS_RIGHT_OR_UP_ << _YAW_ANGLES_);
                       }
                       if (std::abs(pose_status.p_angle[i]) > 20) { //30
                           status |= (pose_status.p_angle[i] > 0) ? (_LIMBS_LEFT_OR_DOWN_ << _PITCH_ANGLES_) : (_LIMBS_RIGHT_OR_UP_ << _PITCH_ANGLES_);
                       }
                       if (std::abs(pose_status.r_angle[i]) > 20) { //30 20
                           status |= (pose_status.r_angle[i] > 0) ? (_LIMBS_LEFT_OR_DOWN_ << _ROLL_ANGLES_) : (_LIMBS_RIGHT_OR_UP_ << _ROLL_ANGLES_);
                       }
                }
		gHumans.peple[i].head.update_status(status);

		status = _LIMBS_INVALID_;
		if (pose_status.value_available[i] & SHOULDER_ANGLE_AVAILABLE) {
		       status = _LIMBS_OK_;
                       if (pose_status.shoulder_angle[i] > 13) { // 15~30 degree
		           status = _LIMBS_LEFT_OR_DOWN_;
                       } else if (pose_status.shoulder_angle[i] < -15) {
		           status = _LIMBS_RIGHT_OR_UP_;
                       }
                }
		gHumans.peple[i].shoulder.update_status(status);

		status = _LIMBS_INVALID_;
		if (pose_status.value_available[i] & BODY_POSITION_AVAILABLE) {
		       status = pose_status.body_status[i];
		}
		gHumans.peple[i].body_position.update_status(status);

		if ((gHumans.peple[i].head.get_status() != _LIMBS_OK_) ||
		    (gHumans.peple[i].shoulder.get_status() != _LIMBS_OK_) ||
		    (gHumans.peple[i].body_position.get_status() != _LIMBS_OK_) ||
		    (gHumans.peple[i].left_eye.get_status() != _LIMBS_OK_) ||
		    (gHumans.peple[i].right_eye.get_status() != _LIMBS_OK_))
		         gHumans.peple[i].is_corrected = false;
		else
		         gHumans.peple[i].is_corrected = true;
	}
	if (pose_status.pose_number < gHumans.numbers) {
	        for (int i = pose_status.pose_number; i < gHumans.numbers; i++) {
			gHumans.peple[i].head.reset_status();
			gHumans.peple[i].shoulder.reset_status();
			gHumans.peple[i].body_position.reset_status();
			gHumans.peple[i].left_eye.reset_status();
			gHumans.peple[i].right_eye.reset_status();
		}
	}

	gHumans.numbers = pose_status.pose_number;

	return 0;
}

static void pooling_human_status() {

        ccai_stream_pipeline *p = NULL;
        std::map<std::string, ccai_stream_pipeline*>::iterator it;

	std::string pose_pipeline = "launcher.pose_estimation";
        it = pipelines.find(pose_pipeline.c_str());
        if (it == pipelines.end()) {
            I("cannot find pipeline: %s, ignore method read\n", pose_pipeline.c_str());
            return;
        }

        p = it->second;

        for (int i = 0; i < MAX_PEOPLE; i++) {
            gHumans.peple[i].head.reset_status();
            gHumans.peple[i].shoulder.reset_status();
            gHumans.peple[i].body_position.reset_status();
            gHumans.peple[i].left_eye.reset_status();
            gHumans.peple[i].right_eye.reset_status();
        }
       while (true) {
	    {
                std::lock_guard<std::mutex> lck(gMtx);
                if (!pooling) return;

                struct ccai_stream_pose_result pose_status;
                if (ccai_stream_pipeline_read(p, (void *)(&pose_status)) == 0) {
	            update_status(pose_status);
	        } else {
                    I("fcgi read pipeline error!\n");
	        }
	    }

	    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
       }
}

static void stop_thread()
{
       {
            std::lock_guard<std::mutex> lck(gMtx);
            pooling = false;
       }

       if (thread_fun.joinable()) thread_fun.join();
}

std::string do_streaming(const char *post_data)
{
	int ret = -1;
	struct json_object *obj = NULL;
	const char *pipe_name = NULL;
	const char *method = NULL;
	const char *parameter = NULL;
	const char *config = NULL;
	std::string read_string;

	I("post_data: %s\n", post_data);

	if ((obj = json_tokener_parse(post_data)) == NULL) {
		I("json_tokener_parse error\n");
		return std::to_string(ret);
	}
	json_object_object_foreach(obj, key, value) {
		if (strncmp(key, "pipeline", 8) == 0) {
			pipe_name = json_object_get_string(value);
		} else if (strncmp(key, "method", 6) == 0) {
			method = json_object_get_string(value);
		} else if (strncmp(key, "parameter", 9) == 0) {
			parameter = json_object_get_string(value);
		} else if (strncmp(key, "config", 9) == 0) {
			config = json_object_get_string(value);
		}
	}
	I("pipeline=%s\n", pipe_name);
	I("method=%s\n", method);
	I("parameter=%s\n", parameter);
	I("config=%s\n", config);

	if (method == NULL || pipe_name == NULL)
		goto out;
	if (strncmp(method, "start", 5) == 0) {
                gDelayTimer.enableTimer = false;
		if (parameter == NULL)
			parameter = DEFAULT_PARAMETER;
		if (start_pipeline(pipe_name, parameter, config) != 0)
			goto out;

		if (strncmp(pipe_name, "launcher.pose_estimation", 24) == 0) {
		        pooling = true;
		        thread_fun = std::thread(pooling_human_status);
		}
	} else if (strncmp(method, "stop", 4) == 0) {
                gDelayTimer.enableTimer = false;
		if (strncmp(pipe_name, "launcher.pose_estimation", 24) == 0)
			stop_thread();
	        if (stop_pipeline(pipe_name, parameter) != 0)
	 	        goto out;
	} else if (strncmp(method, "read", 4) == 0) {
		int status = read_pipeline(pipe_name, read_string);
		if ((status == -1) && (strncmp(pipe_name, "launcher.pose_estimation", 24) == 0)) {
			stop_thread();
	                stop_pipeline(pipe_name, parameter);
			goto out;
		}
	} else if (strncmp(method, "config", 6) == 0) {
		if (config_pipeline(pipe_name, config) != 0)
			goto out;
	}
	ret = 0;
out:
	if (obj)
		json_object_put(obj);

	return read_string.empty() ? std::to_string(ret) : read_string;
}

int main(int argc, char **argv)
{
	ccai_stream_init();

	int err = FCGX_Init();

	if (err) {
		std::string log_info = "FCGX_Init failed: "
			+ std::to_string(err);
		I("%s\n", log_info.c_str());
		return 1;
	}

	FCGX_Request cgi;
	err = FCGX_InitRequest(&cgi, 0, 0);
	if (err) {
		std::string log_info = "FCGX_InitRequest failed: "
			+ std::to_string(err);
		I("%s\n", log_info.c_str());
		return 2;
	}

        gDelayTimer.enableTimer = false;
	while (1) {
		err = FCGX_Accept_r(&cgi);
		if (err) {
			std::string log_info = "FCGX_Accept_r stopped: "
				+ std::to_string(err);
			I("%s\n", log_info.c_str());
			break;
		}

		char *content = FCGX_GetParam("CONTENT_TYPE", cgi.envp);
		if (content == NULL) {
			I("get content error\n");
			std::string result("Status: 404 error\r\n"
					   "Content-Type: text/html\r\n\r\n"
					   "not Acceptable");
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
			continue;
		}

		// get the post_data form fcgi
		char *post_data = NULL;
		char *lenstr = FCGX_GetParam("CONTENT_LENGTH", cgi.envp);
		unsigned long data_length = 1;

		if (lenstr == NULL ||
		    (data_length += strtoul(lenstr, NULL, 10)) > INT32_MAX) {
			I("get length error\n");
			std::string result("Status: "
					   "404 error\r\n"
					   "Content-Type: text/html\r\n\r\n"
					   "get content length error");
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
			continue;
		}

		post_data = (char *)malloc(data_length);
		if (post_data == NULL) {
			I("malloc buffer error\n");
			std::string result("Status: 404 error\r\n"
					   "Content-Type: text/html\r\n\r\n"
					   "malloc buffer error");
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
			continue;
		}
		int ret = FCGX_GetStr(post_data, data_length, cgi.in);
		post_data[ret] = '\0';

		std::string result("Status: 200 OK\r\n"
				   "Content-Type: text/html\r\n"
				   "charset: utf-8\r\n"
				   "X-Content-Type-Options: nosniff\r\n"
				   "X-frame-options: deny\r\n\r\n");

		std::string post_str = post_data;
		std::string sub_str = "";
		sub_str = get_data(post_str, "healthcheck");
		if (sub_str == "1") {
			result += "healthcheck ok";
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
		} else {
			std::string ret = do_streaming(post_data);
			result += ret;
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
		}
		// free memory
		free(post_data);
	}

	return 0;
}
