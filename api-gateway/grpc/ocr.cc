// Copyright (C) 2020 Intel Corporation

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include "vino_ie_pipe.hpp"
#include "grpc_inference_service.h"
#include "ocr.h"

#define TEXT_DETECTION_MODEL	"models/text-detection-0003.xml"
#define TEXT_RECOGNITION_MODEL	"models/text-recognition-0012.xml"

void softmax(std::vector<float> &v)
{
	const size_t dim = 2;
	for (size_t i = 0 ; i < v.size(); i += dim) {
		float max = std::max(v[i], v[i + 1]);
		v[i] = std::exp(v[i] - max);
		v[i + 1] = std::exp(v[i + 1] - max);
		float sum = v[i] + v[i + 1];
		v[i] /= sum;
		v[i + 1] /= sum;
	}
}

bool transpose4d(std::vector<float> &v, const std::vector<size_t> &shape,
		 const std::vector<size_t> &axes)
{
	if (shape.size() != axes.size()) {
		error_log("shape axes must same dim");
		return false;
	}
	for (size_t s : axes) {
		if (s >= shape.size()) {
			error_log("axis must be less than dim of shape.");
			return false;
		}
	}

	size_t total = shape[0] * shape[1] * shape[2] * shape[3];
	std::vector<float> ov(total, 0);

	std::vector<size_t> step {
		shape[axes[1]] * shape[axes[2]] * shape[axes[3]],
		shape[axes[2]] * shape[axes[3]],
		shape[axes[3]],
		1
	};

	std::vector<size_t> id(shape.size());

	size_t si = 0;
	for (id[0] = 0; id[0] < shape[0]; id[0]++) {
		for (id[1] = 0; id[1] < shape[1]; id[1]++) {
			for (id[2] = 0; id[2] < shape[2]; id[2]++) {
				for (id[3]= 0; id[3] < shape[3]; id[3]++) {
					size_t ni = id[axes[0]] * step[0] + \
						    id[axes[1]] * step[1] + \
						    id[axes[2]] * step[2] + \
						    id[axes[3]] * step[3];
					ov[ni] = v[si++];
				}
			}
		}
	}

	v = std::move(ov);

	return true;
}

void slice_set_second_channel(std::vector<float> &v)
{
	std::vector<float> ov(v.size() / 2, 0);

	for (size_t i = 0; i < v.size() / 2; i++)
		ov[i] = v[2 * i + 1];

	v = std::move(ov);
}

int find_root(int p, std::unordered_map<int, int> &mask)
{
	int r = p;
	bool update_flag = false;

	while (mask.at(r) != -1) {
		r = mask.at(r);
		update_flag = true;
	}
	if (update_flag)
		mask[p] = r;

	return r;
}

void join(int p1, int p2, std::unordered_map<int, int> &mask)
{
	int r1 = find_root(p1, mask);
	int r2 = find_root(p2, mask);
	if (r1 == r2)
		return;
	mask[r1] = r2;
}

cv::Mat masks(const std::vector<cv::Point> &ps, int img_w, int img_h,
	      std::unordered_map<int, int> &img_mask)
{

	std::unordered_map<int, int> r_map;
	cv::Mat mask(img_h, img_w, CV_32S, cv::Scalar(0));

	for (const auto &p : ps) {
		int i = find_root(p.x + p.y * img_w, img_mask);
		if (r_map.find(i) == r_map.end())
			r_map.emplace(i, static_cast<int>(r_map.size() + 1));
		mask.at<int>(p.x + p.y * img_w) = r_map[i];
	}

	return mask;
}

cv::Mat get_mask(const std::vector<float> &conn,
		 const std::vector<int> &conn_shape,
		 const std::vector<float> &clss,
		 const std::vector<int> &clss_shape,
		 float conn_threshold, float clss_threshold)
{
	int img_h = clss_shape[1];
	int img_w = clss_shape[2];

	std::vector<uchar> pm(img_h * img_w, 0);
	std::vector<cv::Point> ps;
	std::unordered_map<int, int> img_m;

	for (size_t i = 0; i < pm.size(); i++) {
		pm[i] = clss[i] >= clss_threshold;
		if (pm[i]) {
			ps.emplace_back(i % img_w, i / img_w);
			img_m[i] = -1;
		}
	}

	std::vector<uchar> conn_m(conn.size(), 0);
	for (size_t i = 0; i < conn_m.size(); i++) {
		conn_m[i] = conn[i] >= conn_threshold;
	}

	size_t nps = size_t(conn_shape[3]);
	for (const cv::Point &p : ps) {
		size_t np = 0;
		for (int y = p.y - 1; y <= p.y + 1; y ++) {
			for (int x = p.x - 1; x <= p.x + 1; x ++) {
				if (x == p.x && y == p.y)
					continue;
				if (x < 0 || x >= img_w
				    || y < 0 || y >= img_h) {
					np ++;
					continue;
				}

				size_t i = size_t(y) * size_t(img_w) + \
					   size_t(x);
				uchar pv = pm[i];
				i = (size_t(p.y) * size_t(img_w) + size_t(p.x))\
				    * nps + np;
				uchar cv = conn_m[i];

				if (pv && cv)
					join(p.x + p.y * img_w, x + y * img_w,
					     img_m);
				np ++;
			}
		}
	}
	return masks(ps, img_w, img_h, img_m);
}

void mask_to_rects(const cv::Mat &mask, float min_area, float min_height,
		   cv::Size img_size, std::vector<cv::RotatedRect> &rects)
{
	double min;
	double max;

	cv::minMaxLoc(mask, &min, &max);
	cv::Mat resized;
	cv::resize(mask, resized, img_size, 0, 0, cv::INTER_NEAREST);

	for (int i = 1; i <= static_cast<int>(max); i ++) {
		cv::Mat bbox = resized == i;
		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(bbox, contours, cv::RETR_CCOMP,
				 cv::CHAIN_APPROX_SIMPLE);
		if (contours.empty())
			continue;
		cv::RotatedRect rect = cv::minAreaRect(contours[0]);
		if (std::min(rect.size.width, rect.size.height) < min_height)
			continue;
		if (rect.size.area() < min_area)
			continue;
		rects.emplace_back(rect);
	}
}

void get_rects(std::vector<float> &conn,
	       std::vector<float> &clss,
	       const cv::Size& img_size,
	       float clss_threshold, float conn_threshold,
	       std::vector<cv::RotatedRect> &rects)
{
	std::vector<size_t> conn_shape;
	conn_shape.emplace_back(1);
	conn_shape.emplace_back(16);
	conn_shape.emplace_back(192);
	conn_shape.emplace_back(320);

	if (transpose4d(conn, conn_shape, {0, 2, 3, 1}) == false) {
		error_log("transpose4d conn failed!\n");
		return;
	}
	softmax(conn);
	slice_set_second_channel(conn);

	std::vector<size_t> clss_shape;
	clss_shape.emplace_back(1);
	clss_shape.emplace_back(2);
	clss_shape.emplace_back(192);
	clss_shape.emplace_back(320);

	if (transpose4d(clss, clss_shape, {0, 2, 3, 1}) == false) {
		error_log("transpose4d clss failed!\n");
		return;
	}
	softmax(clss);
	slice_set_second_channel(clss);

	std::vector<int> new_conn_shape(4);
	new_conn_shape[0] = static_cast<int>(conn_shape[0]);
	new_conn_shape[1] = static_cast<int>(conn_shape[2]);
	new_conn_shape[2] = static_cast<int>(conn_shape[3]);
	new_conn_shape[3] = static_cast<int>(conn_shape[1]) / 2;
	std::vector<int> new_clss_shape(4);
	new_clss_shape[0] = static_cast<int>(clss_shape[0]);
	new_clss_shape[1] = static_cast<int>(clss_shape[2]);
	new_clss_shape[2] = static_cast<int>(clss_shape[3]);
	new_clss_shape[3] = static_cast<int>(clss_shape[1]) / 2;

	cv::Mat mask = get_mask(conn, new_conn_shape, clss, new_clss_shape,
				conn_threshold, clss_threshold);
	mask_to_rects(mask, 300.0, 10.0, img_size, rects);
}

void rect_to_points(const cv::RotatedRect &rect,
		    std::vector<cv::Point2f> &points)
{
	cv::Point2f vertices[4];
	rect.points(vertices);
	for (int i = 0; i < 4; i++)
		points.emplace_back(vertices[i].x, vertices[i].y);
}

void left_top_point(const std::vector<cv::Point2f> &points, int *index)
{
	cv::Point2f left1(std::numeric_limits<float>::max(),
			       std::numeric_limits<float>::max());
	cv::Point2f left2(std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max());
	int left_i1 = -1;
	int left_i2 = -1;

	for (size_t i = 0; i < points.size() ; i++) {
		if (left1.x > points[i].x) {
			if (left1.x < std::numeric_limits<float>::max()) {
				left2 = left1;
				left_i2 = left_i1;
			}
			left1 = points[i];
			left_i1 = static_cast<int>(i);
		}
		if (left2.x > points[i].x && points[i] != left1) {
			left2 = points[i];
			left_i2 = static_cast<int>(i);
		}
	}

	if (left2.y < left1.y) {
		left1 = left2;
		left_i1 = left_i2;
	}

	*index = left_i1;
}

cv::Mat crop_img(const cv::Mat &img,
		 const std::vector<cv::Point2f> &rect_points,
		 const cv::Size& size,
		 int i)
{
	cv::Point2f p1 = rect_points[static_cast<size_t>(i)];
	cv::Point2f p2 = rect_points[(i + 1) % 4];
	cv::Point2f p3 = rect_points[(i + 2) % 4];

	cv::Mat crop(size, CV_8UC3, cv::Scalar(0));

	std::vector<cv::Point2f> from { p1, p2, p3 };
	std::vector<cv::Point2f> to { cv::Point2f(0.0f, 0.0f),
		cv::Point2f(static_cast<float>(size.width - 1), 0.0f),
		cv::Point2f(static_cast<float>(size.width - 1),
			    static_cast<float>(size.height - 1))
	};

	cv::Mat affine = cv::getAffineTransform(from, to);
	cv::warpAffine(img, crop, affine, crop.size());

	return crop;
}

std::string ocr(cv::Mat img)
{
	std::vector<float> conn, clss;
	std::vector<std::vector<float>*> raw_results;
	raw_results.push_back(&conn);
	raw_results.push_back(&clss);

	int ret = infer(img, TEXT_DETECTION_MODEL, raw_results);
	if (ret == RT_INFER_ERROR || ret == RT_REMOTE_INFER_OK) {
		error_log("error or remote infer\n");
		return std::string("");
	}
	debug_log("ret=%d, conn.size()=%lu, clss.size()=%lu\n",
		  ret, conn.size(), clss.size());

	std::vector<cv::RotatedRect> rects;
	get_rects(conn, clss, img.size(), 0.8, 0.8, rects);

	// decode detection and do recognition
	std::string ie_result("[");

	for (const auto &rect : rects) {
		if (rect.size == cv::Size2f(0, 0))
			continue;

		std::vector<cv::Point2f> points;
		rect_to_points(rect, points);

		int i = 0;
		left_top_point(points, &i);

		int x = static_cast<int>(points[i].x);
		int y = static_cast<int>(points[i].y);
		int w = static_cast<int>(rect.size.width);
		int h = static_cast<int>(rect.size.height);

		ie_result += "{\"itemcoord\":{\"x\":";
		ie_result += std::to_string(x);
		ie_result += ",\"y\":" + std::to_string(y);
		ie_result += ",\"width\":" + std::to_string(w);
		ie_result += ",\"height\":" + std::to_string(h);
		ie_result += "},";

		// recognition
		// input img grey,  size 120x32
		cv::Mat text = crop_img(img, points, cv::Size(120, 32), i);
		cv::Mat grey;
		cv::cvtColor(text, grey, cv::COLOR_BGR2GRAY);

		// result
		std::vector<float> recognition;
		raw_results.clear();
		raw_results.push_back(&recognition);

		// call infer
		ret = infer(grey, TEXT_RECOGNITION_MODEL, raw_results);

		// decode recognition
		std::string alpha = "0123456789abcdefghijklmnopqrstuvwxyz#";
		std::string result_text = "";

		const int alpha_len = alpha.length();
		const char stop = '#';
		bool follow_stop = false;

		for (std::vector<float>::const_iterator it = recognition.begin();
		     it != recognition.end(); it += alpha_len) {
			// get max confidence symbol
			int i = std::distance(it,
					std::max_element(it, it + alpha_len));
			auto symbol = alpha[i];

			// debug_log("symbol=%c\n", symbol);
			if (symbol == stop) {
				follow_stop = true;
				continue;
			}
			if (result_text.empty() || follow_stop \
			    || symbol != result_text.back()) {
				follow_stop = false;
				result_text += symbol;
			}
		}
		ie_result += "\"itemstring\":\"" + result_text + "\"},";
	}

	if ((!ie_result.empty()) && ie_result.back() == ',')
		ie_result.pop_back();
	ie_result += "]";

	return ie_result;
}
