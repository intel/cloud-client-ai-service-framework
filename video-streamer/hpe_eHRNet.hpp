// Copyright (C) 2021 Intel Corporation

#pragma once

using humanKeypoints = std::vector<cv::Point2f>;

class HpeEHRNet {
public:
    HpeEHRNet(){};
    int hpe_init(const std::string& modelName, float resize_scale, int inputlayer_h,
                 int inputlayer_w, std::vector<unsigned long>& scoreMaps_dim,
		 std::vector<unsigned long>& tagMaps_dim, float confidenceThresh);
    int ccai_preprocess(cv::Mat& input_image, cv::Mat& output_image, std::vector<float>& scales);
    int ccai_postprocess(std::vector<float>& tagMaps_vec,
                         std::vector<float>& scoreMaps_vec,
                         std::vector<float>& scales, std::vector<humanKeypoints>& result);
    bool get_init_status() { return initialized; };
    void set_init_status(bool status) { initialized = status; };
    int get_stride() { return stride; };

protected:

    int modelInputHeight;
    int modelInputWidth;
    float resizeScale;
    float confidenceThreshold;
    bool initialized = false;
    std::vector<size_t> scoreMapsDim;
    std::vector<size_t> tagMapsDim;

    static const int numJoints = 17;
    static const int stride = 32;
    static const int maxNumPeople = 10;

    std::string modelFileName;
};
