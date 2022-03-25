// Copyright (C) 2021 Intel Corporation

#include <algorithm>
#include <string>
#include <vector>
#include <iostream>

#include <opencv2/imgproc/imgproc.hpp>
#include "opencv2/core.hpp"

#include "associative_embedding_decoder.hpp"
#include "hpe_eHRNet.hpp"

static const cv::Vec3f meanPixel = cv::Vec3f::all(128);
static const float detectionThreshold = 0.1f;
static const float tagThreshold = 1.0f;
static const float delta = 0.0f;

int HpeEHRNet::hpe_init(const std::string& modelName, float resize_scale, int inputlayer_h,
                        int inputlayer_w, std::vector<unsigned long>& scoreMaps_dim,
                        std::vector<unsigned long>& tagMaps_dim,float confidenceThresh ) {
    modelFileName = modelName;
    confidenceThreshold = confidenceThresh;
    resizeScale = resize_scale;
    modelInputHeight = inputlayer_h;
    modelInputWidth = inputlayer_w;
    scoreMapsDim.assign(scoreMaps_dim.begin(), scoreMaps_dim.end());
    tagMapsDim.assign(tagMaps_dim.begin(), tagMaps_dim.end());

    initialized = true;

    return 0;
}

int HpeEHRNet::ccai_preprocess(cv::Mat& input_image,
	                       cv::Mat& output_image,
                               std::vector<float>& scales) {
    auto& image = input_image;
    cv::Mat resizedImage;
    cv::resize(image, resizedImage, cv::Size(), resizeScale, resizeScale, cv::INTER_CUBIC);

    int h = resizedImage.rows;
    int w = resizedImage.cols;
    if (!(modelInputHeight - stride < h && h <= modelInputHeight
        && modelInputWidth - stride < w && w <= modelInputWidth)) {
        std::cout << "Chosen model aspect ratio doesn't match image aspect ratio" << std::endl;
    }

    cv::Mat& paddedImage = output_image;
    int bottom = modelInputHeight - h;
    int right = modelInputWidth - w;
    cv::copyMakeBorder(resizedImage, paddedImage, 0, bottom, 0, right,
                       cv::BORDER_CONSTANT, meanPixel);

    scales.push_back(image.cols / static_cast<float>(w));
    scales.push_back(image.rows / static_cast<float>(h));

    return 0;
}

int HpeEHRNet::ccai_postprocess(std::vector<float>& tagMaps_vec,
                                std::vector<float>& scoreMaps_vec,
                                std::vector<float>& scales,
                                std::vector<humanKeypoints>& result) {

    float* tagMapsData = tagMaps_vec.data();
    std::vector<cv::Mat> tagMaps(tagMapsDim[1]);
    for (size_t i = 0; i < tagMapsDim[1]; i++) {
        tagMaps[i] = cv::Mat(tagMapsDim[2], tagMapsDim[3], CV_32FC1, tagMapsData + i * tagMapsDim[2] * tagMapsDim[3]);
    }

    float* scoreMapsData = scoreMaps_vec.data();
    std::vector<cv::Mat> scoreMaps(scoreMapsDim[1]);
    for (size_t i = 0; i < scoreMapsDim[1]; i++) {
        scoreMaps[i] = cv::Mat(scoreMapsDim[2], scoreMapsDim[3], CV_32FC1, scoreMapsData + i * scoreMapsDim[2] * scoreMapsDim[3]);
    }

    std::vector<std::vector<Peak>> allOfPeaks(numJoints);
    for (int i = 0; i < numJoints; i++) {
        findPeaks(scoreMaps, tagMaps, allOfPeaks, i, maxNumPeople, detectionThreshold);
    }

    std::vector<Pose> allOfPoses = matchByTag(allOfPeaks, maxNumPeople, numJoints, tagThreshold);
    for (size_t i = 0; i < scoreMaps.size(); i++) {
        scoreMaps[i] = cv::abs(scoreMaps[i]);
    }

    float outputScale = modelInputWidth / static_cast<float>(scoreMapsDim[3]);
    float scaleX = scales[0] * outputScale;
    float scaleY = scales[1] * outputScale;

    for (size_t i = 0; i < allOfPoses.size(); i++) {
        Pose aPose = allOfPoses[i];
        if (aPose.getMeanScore() <= confidenceThreshold) {
            continue;
        }
        adjustAndRefine(allOfPoses, scoreMaps, tagMaps, i, delta);

	humanKeypoints oneHumanKeypoints;
        for (size_t joint = 0; joint < numJoints; joint++) {
            auto keypoint = aPose.getPeak(joint).keypoint;
            if (keypoint != cv::Point2f(-1, -1)) {
                keypoint.x *= scaleX;
                keypoint.y *= scaleY;
                std::swap(keypoint.x, keypoint.y);
            }
            oneHumanKeypoints.push_back(keypoint);
        }
        result.push_back(oneHumanKeypoints);
    }

    return 0;
}
