// Copyright (C) 2020 Intel Corporation

#define AUTH
#ifdef AUTH
#include <security/pam_appl.h>
#include <fcntl.h>
#include "../fcgi/utils/cpp/fcgi_utils.h"
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>

#include <grpc++/grpc++.h>

#include <inference_engine.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include "vino_ie_pipe.hpp"

#include "inference_service.grpc.pb.h"
#include "grpc_inference_service.h"
#include "ocr.h"
#include "asr.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerReader;

using inference_service::Result;
using inference_service::Input;
using inference_service::Inference;
using inference_service::Empty;
using inference_service::HealthStatus;

#define CLASSIFICATION_MODEL	"models/alexnet.xml"
#define CLASSIFICATION_LABELS	"models/alexnet.labels"
#define FACE_DETECTION_MODEL	"models/face-detection-adas-0001.xml"
#define FACIAL_LANDMARKS_MODEL	"models/facial-landmarks-35-adas-0002.xml"

int infer(cv::Mat img, const char *model, std::vector<std::vector<float>*> &r)
{
	if (img.empty())
		return RT_INFER_ERROR;

	// vino parameters
	std::shared_ptr<cv::Mat> img_ptr(new cv::Mat(img));
	std::vector<std::shared_ptr<cv::Mat>> images;
	images.push_back(img_ptr);

	std::vector<std::vector<float>> additional_input;

	// TODO set params
	std::string params("");
	struct serverParams url_info = {
		"https://api.ai.qq.com/fcgi-bin/image/image_tag", params };

	// call vino
	int ret = vino_ie_pipeline_infer_image(images,
					       additional_input,
					       std::string(model),
					       r,
					       url_info);

	return ret;
}

bool face_detection_ex(cv::Mat img,
		       std::function<void (cv::Rect &)> result_callback)
{
	std::vector<float> detections;
	std::vector<std::vector<float>*> raw_results;
	raw_results.push_back(&detections);

	int ret = infer(img, FACE_DETECTION_MODEL, raw_results);
	if (ret == RT_INFER_ERROR) {
		return false;
	} else if (ret == RT_REMOTE_INFER_OK) {
		// TODO: decode qq clould result
		error_log("TODO: get result from qq clould\n");
		return false;
	}
	debug_log("ret=%d, detections.size()=%lu\n", ret, detections.size());

	// decode vino(OpenVINO) result
	// the model output object_size is 7, lenght is 200
	const int object_size = 7;
	const int results_size = 200;
	// check if the infer result size is correct.
	if (detections.size() != object_size * results_size) {
		error_log("error: incrrect detection.size:  %lu\n",
			  detections.size());
		return false;
	}

	int img_w = img.cols;
	int img_h = img.rows;
	debug_log("img_w: %d, img_h: %d\n", img_w, img_h);
	std::string ie_result;
	for (int i = 0; i < results_size; i++) {
		int offset = i * object_size;
		float image_id = detections[offset];
		if (image_id < 0)
			break;

		// confidence
		if (detections[offset+2] < .5)
			continue;

		debug_log("confidence: %f\n", detections[offset + 2]);

		float fx1 = detections[offset + 3] * img_w;
		float fy1 = detections[offset + 4] * img_h;
		float fx2 = detections[offset + 5] * img_w;
		float fy2 = detections[offset + 6] * img_h;

		// Make square and enlarge for more robust operation of
		// face analytics networks
		float center_x = (fx1 + fx2) / 2;
		float center_y = (fy1 + fy2) / 2;
		// enlarge to 1.2
		float side = std::max(fx2 - fx1,  fy2 - fy1) * 1.2;
		cv::Rect rect(static_cast<int>(std::floor(center_x-side/2)),
			      static_cast<int>(std::floor(center_y-side/2)),
			      static_cast<int>(std::floor(side)),
			      static_cast<int>(std::floor(side)));
		result_callback(rect);
	}

	return true;
}

std::string face_detection(cv::Mat img)
{
	std::string ie_result;

	auto result_callback = [&ie_result] (cv::Rect &rect) {
		// output to json string
		ie_result += "{";
		ie_result += "\"x1\":" + std::to_string(rect.x) + ",";
		ie_result += "\"y1\":" + std::to_string(rect.y) + ",";
		ie_result += "\"x2\":";
		ie_result += std::to_string(rect.x + rect.width) + ",";
		ie_result += "\"y2\":" + std::to_string(rect.y + rect.height);
		ie_result += "},";

		return;
	};

	face_detection_ex(img, result_callback);

	// erase the last char ','
	if (!ie_result.empty())
		ie_result.pop_back();

	return ie_result;
}

std::string facial_landmark(cv::Mat img)
{
	std::vector<std::shared_ptr<cv::Mat>> images;
	int img_w = img.cols;
	int img_h = img.rows;
	std::vector<cv::Rect> image_rects;

	auto result_callback = [&] (cv::Rect &rect) {
		auto clipped_size = rect & cv::Rect(0, 0, img_w, img_h);
		std::shared_ptr<cv::Mat> img_ptr(new cv::Mat(img, clipped_size));
		images.push_back(img_ptr);
		image_rects.push_back(rect);
		return;
	};

	face_detection_ex(img, result_callback);

	for (auto &i: images)
		debug_log("i->cols=%d, i->rows=%d\n", i->cols, i->rows);

	// vino parameters
	std::vector<float> detection;
	std::vector<std::vector<float>*> raw_results;
	raw_results.push_back(&detection);
	std::vector<std::vector<float>> additional_input;
	std::string params("");
	struct serverParams url_info = {
		"https://api.ai.qq.com/fcgi-bin/image/image_tag", params };

	// call vino
	int ret = vino_ie_pipeline_infer_image(images,
					       additional_input,
					       FACIAL_LANDMARKS_MODEL,
					       raw_results,
					       url_info);
	if (ret == RT_INFER_ERROR || ret == RT_REMOTE_INFER_OK) {
		error_log("error or remote infer\n");
		return std::string("");
	}

	debug_log("detection.size()=%lu\n", detection.size());

	// decode
	std::string ie_result;
	const int landmarks_size = 70/2; // define in the model
	for (unsigned long ii = 0; ii < image_rects.size(); ii ++) {
		for (int i = 0; i < landmarks_size; i++) {
			int ix = (ii * landmarks_size + i) * 2;
			int iy = ix + 1;
			int x = static_cast<int>(detection[ix] * \
						 image_rects[ii].width + \
						 image_rects[ii].x);
			int y = static_cast<int>(detection[iy] * \
						 image_rects[ii].height + \
						 image_rects[ii].y);
			ie_result += "{\"x\":" + std::to_string(x) + "," + \
				      "\"y\":" + std::to_string(y) + "},";
		}
	}

	// erase the last char ','
	if (!ie_result.empty())
		ie_result.pop_back();

	return ie_result;
}

std::string classification(cv::Mat img)
{
	std::vector<float> detection;
	std::vector<std::vector<float>*> raw_results;
	raw_results.push_back(&detection);

	int ret = infer(img, CLASSIFICATION_MODEL, raw_results);
	if (ret == RT_INFER_ERROR) {
		return std::string("");
	} else if (ret == RT_REMOTE_INFER_OK) {
		// TODO: decode qq clould result
		return std::string("TODO: get result from qq cloud");
	}
	debug_log("ret=%d, detection.size()=%lu\n", ret, detection.size());

	// decode vino(OpenVINO) result
	// get labels
	std::ifstream label_file;
	label_file.open(CLASSIFICATION_LABELS, std::ios::in);
	std::vector<std::string> labels;
	if (label_file.is_open()) {
		std::string str_line;
		while (std::getline(label_file, str_line))
			labels.push_back(str_line);
	}

	// get up to 10 results
	std::string ie_result("");
	for (int i = 0; i < 10; i++) {
		std::vector<float>::iterator biggest =
			std::max_element(std::begin(detection),
					 std::end(detection));
		// ignore all results of confidence < .5
		if (*biggest < 0.5)
			break;
		int position = std::distance(std::begin(detection), biggest);
		ie_result += "{\"tag_name\":";
		std::string label = labels[position].substr(
					labels[position].find("'"));
		// replace \' to \" to get a correct json string
		std::replace(label.begin(), label.end(), '\'', '\"');
		ie_result += label;
		ie_result += "\"tag_confidence\":";
		ie_result += std::to_string(*biggest) + "},";
		// To find the next biggest, set the confidence to 0.
		*biggest = 0;
	}

	// pop back ','
	if (!ie_result.empty() && ie_result.back() == ',')
		ie_result.pop_back();

	ie_result = "[" + ie_result + "]";

	return ie_result;
}

#define INFERENCEIMPL_IMAGE_SERVICE(service_name, func_name) \
	Status service_name(ServerContext* context, const Input* input, \
			    Result* result) override \
	{ \
		Status st = Authenticate(context); \
		if (!st.ok()) return st; \
		info_log("Call " #service_name "\n"); \
		std::vector<char> v(input->buffer().begin(), \
				    input->buffer().end()); \
		cv::Mat input_data = cv::imdecode(v, cv::IMREAD_COLOR); \
		infer_mutex.lock(); \
		std::string res = func_name(input_data); \
		infer_mutex.unlock(); \
		result->set_json(res); \
		info_log(#service_name " return\n"); \
		return Status::OK; \
	}

class InferenceImpl final : public Inference::Service
{
private:
	std::timed_mutex infer_mutex;
	std::string pam_service;

public:
	InferenceImpl(const std::string& pam)
		: Inference::Service(), pam_service(pam) {}
	INFERENCEIMPL_IMAGE_SERVICE(Classification, classification)
	INFERENCEIMPL_IMAGE_SERVICE(FaceDetection, face_detection)
	INFERENCEIMPL_IMAGE_SERVICE(FacialLandmark, facial_landmark)
	INFERENCEIMPL_IMAGE_SERVICE(OCR, ocr)

	Status ASR(ServerContext* context, const Input* input,
		   Result* result) override
	{
		Status st = Authenticate(context);
		if (!st.ok()) return st;

		info_log("Call ASR\n");
		infer_mutex.lock();
		std::string res = asr(input->buffer());
		infer_mutex.unlock();
		result->set_json(res);
		info_log("ASR return\n");

		return Status::OK;
	}

	Status HealthCheck(ServerContext* context, const Empty* empty,
		   HealthStatus* status) override
	{
		Status st = Authenticate(context);
		if (!st.ok()) return st;

		if (infer_mutex.try_lock_for(std::chrono::seconds(10))) {
			infer_mutex.unlock();
			status->set_status(HealthStatus::SUCCESS);
		} else {
			status->set_status(HealthStatus::TIMEOUT);
		}

		return Status::OK;
	}

private:
#ifdef AUTH
	Status Authenticate(ServerContext* context) const {
		if (pam_service.empty())  // authentication is disabled
			return Status::OK;

		// extract encoded credentical from metadata
		auto metadata = context->client_metadata();
		auto it = metadata.find(grpc::string_ref("authorization"));
		if (it == metadata.end() || !it->second.starts_with("Basic "))
			return Status(grpc::StatusCode::UNAUTHENTICATED, "Authenticate failed");
		grpc::string_ref credential = it->second.substr(6);
#if !__OPTIMIZE__
		debug_log("credential: %.*s", (int)credential.size(), credential.data());
#endif

		// decode credentical
		std::string user = DecodeBase(credential.data(), credential.size());
		size_t s = user.find(':');
		if (s == std::string::npos)
			return Status(grpc::StatusCode::UNAUTHENTICATED, "Authenticate failed");
		std::string pass = user.substr(s + 1);
		user.resize(s);

		// authenticate with PAM
		Status st(grpc::StatusCode::PERMISSION_DENIED, "Permission denied");
		if (pam_auth(pam_service.c_str(), user.c_str(), pass.c_str()))
			st = Status::OK;
		return st;
	}

	static int pam_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr) {
		char *p;

		*resp = (pam_response *)calloc(num_msg, sizeof(pam_response));
		if (*resp == NULL)
			return PAM_SUCCESS;

		for (int i = 0; i < num_msg; ++i) {
			if (msg[i]->msg_style == PAM_PROMPT_ECHO_ON ||
			    msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
				if ((p = strdup((char*)appdata_ptr)) != NULL)
					(*resp)[i].resp = p;
			}
		}
		return PAM_SUCCESS;
	}

	static int pam_auth(const char* service, const char* username, const char* password) {
		pam_handle_t *pamh = NULL;
		struct pam_conv conv = { pam_conv, NULL };
		*(const char **)&conv.appdata_ptr = password;
		if (pam_start(service, username, &conv, &pamh) != PAM_SUCCESS)
			return 0;

		const int flags = PAM_SILENT | PAM_DISALLOW_NULL_AUTHTOK;
		int rc;
		if ((rc = pam_set_item(pamh, PAM_RHOST, "")) != PAM_SUCCESS ||
		    (rc = pam_authenticate(pamh, flags)) != PAM_SUCCESS  ||
		    (rc = pam_acct_mgmt(pamh, flags)) != PAM_SUCCESS)
			error_log("pam: %s", pam_strerror(pamh, rc));
		pam_end(pamh, rc);
		return rc == PAM_SUCCESS;
	}
#else
	Status Authenticate(ServerContext* context) const {
		return Status::OK;
	}
#endif
};

void run_server(const char *port, const char* pam)
{
	std::string server_address("0.0.0.0:");
	server_address += port;

	InferenceImpl *service = new InferenceImpl(pam);
	ServerBuilder *builder = new ServerBuilder;
	builder->SetMaxReceiveMessageSize(-1);
	builder->AddListeningPort(server_address,
				  grpc::InsecureServerCredentials());
	builder->RegisterService(service);
	std::unique_ptr<Server> server(builder->BuildAndStart());
	std::cout << "Server listening on " << server_address << std::endl;
	server->Wait();

	delete builder;
	delete service;
}

int main(int argc, char **argv)
{
	const char *pam="";
	const char *port="50001";
	int opt;
	while ((opt = getopt(argc, argv, "a:h")) != -1) {
		int dirfd;
		switch (opt) {
		case 'h':
			printf("usage: %s [-a pam_service] [port]\n", argv[0]);
			exit(0);
		case 'a':
			// validate pam service name
			if (!strchr(optarg, '/') &&
			    (dirfd = open("/etc/pam.d", O_RDONLY | O_DIRECTORY | O_NOFOLLOW)) >= 0) {
				if (!faccessat(dirfd, optarg, R_OK, AT_EACCESS | AT_SYMLINK_NOFOLLOW)) {
					pam = optarg;
					debug_log("auth with pam service: %s", pam);
				}
				close(dirfd);
			}
			if (pam[0]) break;
			fprintf(stderr, "invalid pam service name: %s\n", optarg);
		default:
			exit(1);
		}
	}
	if (optind < argc)
		port = argv[optind];

	run_server(port, pam);

	return 0;
}
