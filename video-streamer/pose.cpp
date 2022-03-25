// Copyright (C) 2021 Intel Corporation

#include <ccai_stream_plugin.h>
#include <ccai_stream_utils.h>
#include <ccai_stream.h>

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <opencv2/opencv.hpp>
#include <gst/videoanalytics/video_frame.h>
#include <sys/time.h>
#include <mutex>

#include <vino_ie_pipe.hpp>
#include <hpe_eHRNet.hpp>

#define D(fmt, ...) CS_D("[pose]: " fmt, ##__VA_ARGS__);
#define I(fmt, ...) CS_I("[pose]: " fmt, ##__VA_ARGS__);
#define E(fmt, ...) CS_E("[pose]: " fmt, ##__VA_ARGS__);

static std::string human_model_file = "/opt/fcgi/cgi-bin/models/human-pose-estimation-0007.xml";
static std::string eyestate_model_file = "/opt/fcgi/cgi-bin/models/open-closed-eye-0001.xml";

static HpeEHRNet gHpeModel;
static struct ccai_stream_pose_result gPoseInferResult, gReadResult;
static cv::Point2i gNose[MAX_PEOPLE];
static cv::Point2i gBodyRef[MAX_PEOPLE];
static std::mutex gMtx;

static const cv::Scalar colors[18] = { //keypoints number is 18
    cv::Scalar(255, 0, 0), cv::Scalar(255, 85, 0), cv::Scalar(255, 170, 0),
    cv::Scalar(255, 255, 0), cv::Scalar(170, 255, 0), cv::Scalar(85, 255, 0),
    cv::Scalar(0, 255, 0), cv::Scalar(0, 255, 85), cv::Scalar(0, 255, 170),
    cv::Scalar(0, 255, 255), cv::Scalar(0, 170, 255), cv::Scalar(0, 85, 255),
    cv::Scalar(0, 0, 255), cv::Scalar(85, 0, 255), cv::Scalar(170, 0, 255),
    cv::Scalar(255, 0, 255), cv::Scalar(255, 0, 170), cv::Scalar(255, 0, 85)
};

static std::vector<std::pair<int, int>> modelKeypointIdPairs = {
    {15, 13}, {13, 11}, {16, 14}, {14, 12}, {11, 12}, {5, 11},
    {6, 12}, {5, 6}, {5, 7}, {6, 8}, {7, 9}, {8, 10},
    {1, 2}, {0, 1}, {0, 2}, {1, 3}, {2, 4}, {3, 5}, {4, 6}
};

cv::Rect createEyeBoundingBox(const cv::Point2i& p1,
                              const cv::Point2i& p2,
                              float scale = 1.8) {
    cv::Rect eyeBox;
    float size = static_cast<float>(cv::norm(p1 - p2));
    eyeBox.width = static_cast<int>(scale * size);
    eyeBox.height = eyeBox.width;

    auto midpoint = (p1 + p2) / 2;
    eyeBox.x = midpoint.x - (eyeBox.width / 2);
    eyeBox.y = midpoint.y - (eyeBox.height / 2);

    return eyeBox;
}

void rotateImage(const cv::Mat& srcImage,
                 cv::Mat& dstImage,
                 float angle) {
    auto w = srcImage.cols;
    auto h = srcImage.rows;
    cv::Size size(w, h);

    cv::Point2f center(static_cast<float>(w / 2), static_cast<float>(h / 2));
    auto rotMatrix = cv::getRotationMatrix2D(center, static_cast<double>(angle), 1);
    cv::warpAffine(srcImage, dstImage, rotMatrix, size, 1, cv::BORDER_REPLICATE);
}

int estimate_eye_state(const cv::Mat& srcImage,
	               std::vector<cv::Point2i>& faceLandmarks,
		       float& roll, int& index) {

    bool leftEyeState = false;
    bool rightEyeState = false;
  #ifdef _USE_BGRA_
    cv::Mat frame;
    cv::cvtColor(srcImage, frame, cv::COLOR_BGRA2BGR);
  #else
    cv::Mat frame = srcImage.clone();
  #endif
    std::vector<std::vector<float>> additionalInput;
    std::vector<float> rawDetectionResult;
    std::vector<std::vector<float>*> rawDetectionResults;
    rawDetectionResults.push_back(&rawDetectionResult);

    auto leftEyeBoundingBox = createEyeBoundingBox(faceLandmarks[0], faceLandmarks[1]);
    if (leftEyeBoundingBox.area()) {
        auto leftEyeImage(cv::Mat(frame, leftEyeBoundingBox));
        cv::Mat leftEyeImageRotated;
        rotateImage(leftEyeImage, leftEyeImageRotated, roll);
        leftEyeImage = leftEyeImageRotated;

        additionalInput.clear();
        rawDetectionResult.clear();
        vino_ie_video_infer_frame(leftEyeImage, additionalInput, eyestate_model_file, rawDetectionResults);   
        leftEyeState = rawDetectionResult[0] < rawDetectionResult[1];
        gPoseInferResult.value_available[index] |= LEFT_EYE_AVAILABLE;
        gPoseInferResult.left_eye_state[index] = (int)leftEyeState;
    }

  #ifdef _USE_BGRA_
    cv::cvtColor(srcImage, frame, cv::COLOR_BGRA2BGR);
  #else
    frame = srcImage.clone();
  #endif   

    auto rightEyeBoundingBox = createEyeBoundingBox(faceLandmarks[2], faceLandmarks[3]);
    if (rightEyeBoundingBox.area()) {
        auto rightEyeImage(cv::Mat(frame, rightEyeBoundingBox));
        cv::Mat rightEyeImageRotated;
        rotateImage(rightEyeImage, rightEyeImageRotated, roll);
        rightEyeImage = rightEyeImageRotated;

        additionalInput.clear();
        rawDetectionResult.clear();
        vino_ie_video_infer_frame(rightEyeImage, additionalInput, eyestate_model_file, rawDetectionResults);   
        rightEyeState = rawDetectionResult[0] < rawDetectionResult[1];
        gPoseInferResult.value_available[index] |= RIGHT_EYE_AVAILABLE;
        gPoseInferResult.right_eye_state[index] = (int)rightEyeState;
    } 

    return 0;
}

static int analyzeHumanKeypoints(std::vector<humanKeypoints>& result, cv::Mat& outputImg) {
    const int stickWidth = 4;
    const cv::Point2f absentKeypoint(-1.0f, -1.0f);

    for (int i = 0; i < MAX_PEOPLE; i++) { 
        gNose[i].x = 0;
        gNose[i].y = 0;
	gBodyRef[i].x = 0;
	gBodyRef[i].y = 0;
    }

    int pose_index = 0;
    for (auto& aHuman : result) {
	if (pose_index >= MAX_PEOPLE) {
            D("more than 3 people, break! \n");
            break;
	}

        bool hasNose = false;
        bool hasEyes = false;
	bool hasShoulder = false;
	int shoulderAngle = 0;
	cv::Point middle_shoulder(0, 0);
        for (const auto& keypointIdPair : modelKeypointIdPairs) {
            std::pair<cv::Point2f, cv::Point2f> aKeypointPair(aHuman[keypointIdPair.first],
                    aHuman[keypointIdPair.second]);
            if (aKeypointPair.first == absentKeypoint
                    || aKeypointPair.second == absentKeypoint) {
                continue;
            }

            if (keypointIdPair.first == 0) {
                gNose[pose_index] = aKeypointPair.first;
		hasNose = true;
	    }

            //check shoulder level, (line between left and right shoulder)
            if ((keypointIdPair.first == 5) && (keypointIdPair.second == 6)) {

                cv::Point difference = aKeypointPair.first - aKeypointPair.second;
                shoulderAngle = static_cast<int>(std::atan2(difference.y, difference.x) * 180 / CV_PI);

		gPoseInferResult.shoulder_angle[pose_index] = shoulderAngle;
                gPoseInferResult.value_available[pose_index] |= SHOULDER_ANGLE_AVAILABLE;

                cv::line(outputImg, aKeypointPair.first, aKeypointPair.second, colors[keypointIdPair.second], stickWidth);
		hasShoulder = true;
                middle_shoulder = (aKeypointPair.first + aKeypointPair.second) / 2;
            }

            //check eye level, (line between left and right eye)
            if ((keypointIdPair.first == 1) && (keypointIdPair.second == 2)) {

                cv::Point difference = aKeypointPair.first - aKeypointPair.second;
                int angle = static_cast<int>(std::atan2(difference.y, difference.x) * 180 / CV_PI);

                gPoseInferResult.header_angle[pose_index] = angle;
                gPoseInferResult.value_available[pose_index] |= HEADER_ANGLE_AVAILABLE;

                cv::line(outputImg, aKeypointPair.first, aKeypointPair.second, colors[keypointIdPair.second], stickWidth);
                hasEyes = true;

		gBodyRef[pose_index] = (aKeypointPair.first + aKeypointPair.second) / 2;
            }
        }

	if (hasNose && hasShoulder)
            cv::line(outputImg, gNose[pose_index], middle_shoulder, colors[6], stickWidth);

	if (!hasNose && hasEyes)
            gNose[pose_index] = gBodyRef[pose_index];

        if (!hasEyes && hasShoulder) {
            gBodyRef[pose_index] = middle_shoulder;
        }

	if (hasEyes || hasShoulder) {
            // check body status from ref pixel
            int width = outputImg.cols;
            int left_boundary = 0.25 * width;
            int right_boundary = 0.70 * width;
            if (gBodyRef[pose_index].x < left_boundary)
                gPoseInferResult.body_status[pose_index] = 1;
            else if (gBodyRef[pose_index].x > right_boundary)
                gPoseInferResult.body_status[pose_index] = 2;
            else
                gPoseInferResult.body_status[pose_index] = 0;
            gPoseInferResult.value_available[pose_index] |= BODY_POSITION_AVAILABLE;

  	    pose_index++;
	} else {
	    D("human_pose not find nose and eyes! \n");
	}
    } // end for(auto pose

    gPoseInferResult.pose_number = pose_index;

    return 0;
}

static int human_pose_estimation(cv::Mat &oneFrame) {

    std::vector<std::vector<float>> additionalInput;
    std::vector<float> scoreMaps;
    std::vector<float> tagMaps;
    std::vector<std::vector<float>*> rawDetectionResults;
    rawDetectionResults.push_back(&tagMaps);
    rawDetectionResults.push_back(&scoreMaps);

    std::vector<std::shared_ptr<cv::Mat>> images;

   #ifdef _USE_BGRA_
    cv::Mat frame;
    cv::cvtColor(oneFrame, frame, cv::COLOR_BGRA2BGR);
   #else
    cv::Mat& frame = oneFrame;
   #endif

    if (frame.empty()) {
	std::cout << "could not read an image!" << std::endl;
        return -1;
    }

    if (!gHpeModel.get_init_status()) {
        //init hpe model
        double aspectRatio = frame.cols / static_cast<double>(frame.rows);

        struct mock_data model_IOInfo;
        struct modelParams model{human_model_file, true, aspectRatio, gHpeModel.get_stride()};
        vino_ie_video_infer_init(model, "CPU", model_IOInfo);

	auto input_iter = model_IOInfo.inputBlobsInfo.begin();
	int model_input_w = 0; 
	int model_input_h = 0;
	if ((model_IOInfo.inputBlobsInfo.size() > 0) && (input_iter->second.size() == 4)) {
	    model_input_w = input_iter->second[3];
	    model_input_h = input_iter->second[2];
	} else {
            std::cout << "input init error!" << std::endl;
	    return -1;
	}

	mock_OutputsDataMap& output_info = model_IOInfo.outputBlobsInfo;
	if (output_info.size() < 2) {
            std::cout << "output init error!" << std::endl;
	    return -1;
	}

	auto& tagMaps_dim = output_info.begin()->second;
	auto& scoreMaps_dim = (++output_info.begin())->second;

        float scale = std::min(model_input_h / static_cast<float>(frame.rows),
                               model_input_w / static_cast<float>(frame.cols));

        gHpeModel.hpe_init(human_model_file, scale, model_input_h, model_input_w, scoreMaps_dim, tagMaps_dim, (float)0.1); //org=0.5
    }

    std::vector<float> scale_info;
    cv::Mat frame_preprocessed;
    gHpeModel.ccai_preprocess(frame, frame_preprocessed, scale_info);

    int res = vino_ie_video_infer_frame(frame_preprocessed, additionalInput, human_model_file, rawDetectionResults);   

    std::vector<humanKeypoints> result;
    gHpeModel.ccai_postprocess(tagMaps, scoreMaps, scale_info, result);

    analyzeHumanKeypoints(result, oneFrame);

    return 0;
}

static int draw_result(cv::Mat& image, bool is_human_model_ok) {

    struct ccai_stream_pose_result& pose_status = gPoseInferResult;

    if (pose_status.pose_number == 0)
	return 0;

    bool ng_result = false;
    if (!is_human_model_ok) {
        ng_result = true;
	goto draw_end;
    }

    for (int i = 0; i < pose_status.pose_number; i++) {

        if (((pose_status.value_available[i] & Y_P_R_AVAILABLE) == 0) ||
	   (std::abs(pose_status.y_angle[i]) > 20) ||
	   (std::abs(pose_status.p_angle[i]) > 20) ||
	   (std::abs(pose_status.r_angle[i]) > 20)) { //30
	    ng_result = true;
	    break;
        }

        if (((pose_status.value_available[i] & SHOULDER_ANGLE_AVAILABLE) == 0) ||
	    (pose_status.shoulder_angle[i] > 13) ||
	    (pose_status.shoulder_angle[i] < -15)) { // 15~30 degree
	    ng_result = true;
	    break;
        }

        if (((pose_status.value_available[i] & BODY_POSITION_AVAILABLE) == 0) ||
	    (pose_status.body_status[i] != 0)) {
            ng_result = true;
            break;
        }

	int double_eyes = LEFT_EYE_AVAILABLE | RIGHT_EYE_AVAILABLE;
	if (((pose_status.value_available[i] & double_eyes) == double_eyes) &&
	    (pose_status.left_eye_state[i] == 0) &&
	    (pose_status.right_eye_state[i] == 0)) {
            ng_result = true;
            break;
        }
    }

draw_end:
    if (ng_result) {
        std::string s = "Incorrect Pose";
        cv::putText(image, cv::format("%s", s.c_str()),
                    cv::Point(static_cast<int>(100), static_cast<int>(100)),
                    cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(0, 0, 255), 2);
    }

    return 0;
}

static GstPadProbeReturn pose_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {

    memset(&gPoseInferResult, 0, sizeof(gPoseInferResult));

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        return GST_PAD_PROBE_OK;

    auto gstBuffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (gstBuffer == NULL)
        return GST_PAD_PROBE_OK;

    GVA::VideoFrame videoFrame(gstBuffer, caps);

    GstMapInfo mapBuf;
    if (!gst_buffer_map(gstBuffer, &mapBuf, GST_MAP_READ)) {
        gst_caps_unref(caps);
        return GST_PAD_PROBE_OK;
    }

    gint width = videoFrame.video_info()->width;
    gint height = videoFrame.video_info()->height;

  #ifdef _USE_BGRA_
    cv::Mat mat(height, width, CV_8UC4, mapBuf.data);
  #else  
    cv::Mat mat(height, width, CV_8UC3, mapBuf.data);
  #endif
    cv::Mat mat_copy = mat.clone();

    human_pose_estimation(mat);

    int pose_index = -1;
    bool human_pose_result = true;
    std::vector<int> select_id;
    std::vector<int> keep_id;

    if (gPoseInferResult.pose_number == 0) {
        human_pose_result = false;
        pose_index = 0;
    } else {
        for (int i = 0; i < gPoseInferResult.pose_number; i++)
	    select_id.push_back(i);
    }

    std::vector<cv::Point2i> faceLandmarks;
    for (GVA::RegionOfInterest &roi : videoFrame.regions()) {
        float head_angle_r = 0, head_angle_p = 0, head_angle_y = 0;
        faceLandmarks.clear(); 
        auto rect = roi.rect();
	cv::Rect cv_rect(rect.x, rect.y, rect.w, rect.h);

	if (human_pose_result) {
	    if (select_id.empty()) {
		break;
	    }

	    int ok_id = -1;
	    for (auto it = select_id.begin(); it != select_id.end(); ) {
	        if (cv_rect.contains(gNose[*it])) {
		    ok_id = *it;
                    select_id.erase(it);
	            break;
		} else {
		    it++;
		}
	    }

	    if (ok_id != -1)
		pose_index = ok_id;
	    else
                continue;

	    keep_id.push_back(pose_index);
	}

        for (auto tensor : roi.tensors()) {
  	    std::string model_name = tensor.model_name();
	    std::string layer_name = tensor.layer_name();

	    std::vector<float> data = tensor.data<float>();
            if (layer_name == "align_fc3") {
                for (guint i = 0; i < data.size() / 2; i++) {
                    int x_lm = rect.x + rect.w * data[2 * i];
                    int y_lm = rect.y + rect.h * data[2 * i + 1];

                    faceLandmarks.push_back(cv::Point2i(x_lm, y_lm));
                }
            }

            if (layer_name.find("angle_r") != std::string::npos) {
                head_angle_r = data[0];
		gPoseInferResult.r_angle[pose_index] = head_angle_r; 
                gPoseInferResult.value_available[pose_index] |= Y_P_R_AVAILABLE;
            }
            if (layer_name.find("angle_p") != std::string::npos) {
                head_angle_p = data[0];
		gPoseInferResult.p_angle[pose_index] = head_angle_p; 
                gPoseInferResult.value_available[pose_index] |= Y_P_R_AVAILABLE;
            }
            if (layer_name.find("angle_y") != std::string::npos) {
                head_angle_y = data[0];
		gPoseInferResult.y_angle[pose_index] = head_angle_y; 
                gPoseInferResult.value_available[pose_index] |= Y_P_R_AVAILABLE;
            }
        }

        if (!faceLandmarks.empty()) {
            estimate_eye_state(mat_copy, faceLandmarks, head_angle_r, pose_index);  
        }

	if (!human_pose_result) {
            pose_index++;
	    gPoseInferResult.pose_number = pose_index;
	    if (pose_index == MAX_PEOPLE) break;
	}
    } // end for(GVA)

    //if no matched, only keep the first one element
    if ((keep_id.empty()) && (gPoseInferResult.pose_number != 0)) {
	keep_id.push_back(0);
    }

    // if no update, keep original status
    if (!keep_id.empty()) {
        std::lock_guard<std::mutex> lck(gMtx);
	int element_num = static_cast<int>(keep_id.size());
	for (int i = 0; i < element_num; i++) {
	    int &j = keep_id[i];
	    gReadResult.value_available[i] = gPoseInferResult.value_available[j];
	    gReadResult.shoulder_angle[i] = gPoseInferResult.shoulder_angle[j];
	    gReadResult.header_angle[i] = gPoseInferResult.header_angle[j];
	    gReadResult.y_angle[i] = gPoseInferResult.y_angle[j];
	    gReadResult.p_angle[i] = gPoseInferResult.p_angle[j];
	    gReadResult.r_angle[i] = gPoseInferResult.r_angle[j];
	    gReadResult.left_eye_state[i] = gPoseInferResult.left_eye_state[j];
	    gReadResult.right_eye_state[i] = gPoseInferResult.right_eye_state[j];
	    gReadResult.body_status[i] = gPoseInferResult.body_status[j];
	}
	gReadResult.pose_number = element_num;
    }

    draw_result(mat, human_pose_result);

    gst_buffer_unmap(gstBuffer, &mapBuf);
    gst_caps_unref(caps);
    GST_PAD_PROBE_INFO_DATA(info) = gstBuffer;

    /*struct timeval end;
    gettimeofday(&end, NULL);
    D("end tv_sec:%ld \n", end.tv_sec);
    D("end tv_usec:%ld \n", end.tv_usec);*/

    return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn facedet_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {

    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer == NULL)
        return GST_PAD_PROBE_OK;

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        return GST_PAD_PROBE_OK;

    GVA::VideoFrame video_frame(buffer, caps);

    int roi_index = 0;
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {

        auto rect = roi.rect();
        int bb_width = rect.w;
        int bb_height = rect.h;
	
        rect.x -= static_cast<int>(0.067 * bb_width);
        rect.y -= static_cast<int>(0.028 * bb_height);

        rect.w += static_cast<int>(0.15 * bb_width);
        rect.h += static_cast<int>(0.13 * bb_height);

        if (rect.w < rect.h) {
            auto dx = (rect.h - rect.w);
            rect.x -= dx / 2;
            rect.w += dx;
        } else {
            auto dy = (rect.w - rect.h);
            rect.y -= dy / 2;
            rect.h += dy;
        }
	
	std::string roi_name = "detection_adjust" + std::to_string(roi_index);
        video_frame.add_region(rect.x, rect.y, rect.w, rect.h, roi_name, 0.5, false);
	video_frame.remove_region(roi);
	roi_index++;
    }

    gst_caps_unref(caps);
    GST_PAD_PROBE_INFO_DATA(info) = buffer;

    return GST_PAD_PROBE_OK;

}

static int read_result_from_pipe(struct ccai_stream_pipeline_desc *desc, void *user_data) {
    if (desc == NULL || user_data == NULL) {
        E("user_data empty!\n");
        return -1;
    }

    struct ccai_stream_pose_result *pInferResult = reinterpret_cast<struct ccai_stream_pose_result*>(user_data);
    {
	std::lock_guard<std::mutex> lck(gMtx);
        *pInferResult = gReadResult;
    }

    return 0;
}

static int create(struct ccai_stream_pipeline *pipe,
		  struct ccai_stream_probe_desc *desc, void *user_data) {
	D("create\n");

	memset(&gPoseInferResult, 0, sizeof(gPoseInferResult));
	memset(&gReadResult, 0, sizeof(gReadResult));
	memset(&gNose, 0, sizeof(gNose));
	memset(&gBodyRef, 0, sizeof(gBodyRef));

        ccai_stream_set_read_func(pipe, read_result_from_pipe);

	if (ccai_stream_pipeline_add_probe_callback(pipe, "facedet", "src",
						    facedet_callback,
						    desc) != 0)
	    return -1;

	if (ccai_stream_pipeline_add_probe_callback(pipe, "facial", "src",
						    pose_callback,
						    desc) != 0)
	    return -1;


	struct mock_data model_IOInfo;
        struct modelParams model{eyestate_model_file, false, 0, 0};
        vino_ie_video_infer_init(model, "CPU", model_IOInfo);
	
	gHpeModel.set_init_status(false);

	return 0;
}

static struct ccai_stream_probe_desc desc = {
	"launcher.pose_estimation", "pose", create, NULL
};

static int pose_init() {
	D("init pose!\n");
	if (ccai_stream_add_probe(&desc) != 0) {
		E("failed to add probe\n");
		return -1;
	}

	return 0;
}

static void pose_exit() {
	D("exit\n");
}

CCAI_STREAM_PLUGIN_DEFINE(pose, "1.0",
			  CCAI_STREAM_PLUGIN_PRIORITY_DEFAULT - 10,
			  pose_init, pose_exit)

