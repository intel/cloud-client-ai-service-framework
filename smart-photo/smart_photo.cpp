#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/limits.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include "vino_ie_pipe.hpp"
#include "ccai_smart_photo.h"

#include "smart_photo_log.h"
#include "smart_photo.h"
#include "db.h"

#define MODEL_DIR                "/opt/fcgi/cgi-bin/models"
#define FACE_DETECTION_MODEL     MODEL_DIR"/face-detection-adas-0001.xml"
#define FACE_REIDENTIFICATION_MODEL	\
	MODEL_DIR"/face-recognition-resnet100-arcface-onnx.xml"
#define HEAD_POSE_MODEL          MODEL_DIR"/head-pose-estimation-adas-0001.xml"
#define SEGMENTATION_MODEL       MODEL_DIR"/hrnet-v2-c1-segmentation.xml"
#define SEGMENTATION_LABELS      MODEL_DIR"/hrnet-v2-c1-segmentation.labels"

#define FACE_DETECTION_CONFIDENCE	0.8
#define SEGMENTATION_CONFIDENCE		0.9
#define FACE_MIN_PERCENT 		0.1
#define HEAD_POSE_MAX_ANGLE		30.0

struct match_face {
	struct smart_photo *sp;
	std::vector<int64_t> ids;
	cv::Mat m;
};

typedef int DB_LIST_FUNCTION(sqlite3 *, DB_EXEC_CALLBACK, void *);

int __ccai_sp_debug_log_enabled = 0;
syslog::ostream * __ccai_sp_syslog = NULL;
int __ccai_sp_log_target = LOG_TARGET_SYSLOG;

int ccai_sp_init(void **sp_handle, const char *db_path)
{
	D();

	struct smart_photo *sp = NULL;
	int rc = -1;

	if (sp_handle == NULL)
		goto err;

	sp = new smart_photo;

	if ((rc = db_init(&sp->db, db_path)) != 0) {
		D(<< "db_init error:" << rc);
		goto err;
	}

	rc = 0;
	*sp_handle = sp;
	return rc;
err:
	if (sp) delete sp;
	return rc;
}

static int wait_scan_thread_stop(struct smart_photo *sp)
{
	if (!sp->scan_runing)
		return 0;

	pthread_join(sp->scan_thread, NULL);

	return 0;
}


int ccai_sp_release(void *sp_handle)
{
	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;

	ccai_sp_stop_scan(sp);

	smart_photo_lock(sp);
	// db
	if (db_finit(sp->db) != 0)
		E(<< "db_finit error");
	sp->db = NULL;
	smart_photo_unlock(sp);

	// free memory
	free(sp);

	return 0;
}

static void init() __attribute__((constructor));
void init()
{
	__ccai_sp_syslog = new syslog::ostream;

	// set log target, default target is syslog
	assert(__ccai_sp_log_target == LOG_TARGET_SYSLOG);

	char *simlib_log_target = std::getenv("CCAI_SPLIB_LOG_TARGET");
	if (simlib_log_target != NULL) {
		if (strncmp(simlib_log_target, "syslog", 6) != 0) {
			__ccai_sp_log_target = LOG_TARGET_COUT;
		}
	}

	char *debug = std::getenv("CCAI_SPLIB_DEBUG");
	if (debug == NULL)
		return;

	try {
		__ccai_sp_debug_log_enabled = std::stoi(debug);
	} catch (...) {
		fprintf(stderr, "[SPLIB][ERROR] invalid environment variable"
				" CCAI_SPLIB_DEBUG\n");
	}

	if (__ccai_sp_debug_log_enabled) {
		setvbuf(stdout, NULL, _IOLBF, 1024);
		setvbuf(stderr, NULL, _IONBF, 1024);
		fprintf(stdout, "[SPLIB][init] loaded\n");
	}
}

static int add_photo(struct smart_photo *sp, const std::string &path)
{
	D(<< path);

	char pp[PATH_MAX];

	if (realpath(path.c_str(), pp) == NULL) {
		E(<< "add_photo failed, path=" << path << " errno=" << errno);
		return -1;
	}

	int r = db_changed_table_add(sp->db, pp);
	if (r != 0) {
		E(<< "db error: db_changed_table_add");
		return -1;
	}

	return 0;
}

int ccai_sp_add_dir(void *sp_handle, const char *dir)
{
	DIR *d = NULL;
	if ((d = opendir(dir)) == NULL) {
		E(<< "Cannot open dir: " << dir << " to scan");
		return -1;
	}

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;
	smart_photo_auto_lock(sp);

	// begin a transaction to quickly insert rows to db
	db_transaction_begin(sp->db);
	struct dirent *de = NULL;
	while ((de = readdir(d)) != NULL) {
		const std::string dot = ".";
		const std::string dot2 = "..";
		if (dot == de->d_name || dot2 == de->d_name)
			continue;

		std::string p(dir);
		p += '/';
		p += de->d_name;
		add_photo(sp, p);
	}
	db_transaction_end(sp->db);

	return 0;
}

int ccai_sp_add_file(void *sp_handle, const char *file)
{
	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;
	smart_photo_auto_lock(sp);

	add_photo(sp, file);

	return 0;
}

int ccai_sp_remove_file(void *sp_handle, const char *file)
{
	D();

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;
	smart_photo_auto_lock(sp);

	return db_photo_table_remove(sp->db, file);
}

int ccai_sp_move_file(void *sp_handle, const char *from, const char *to)
{
	D();

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;
	smart_photo_auto_lock(sp);

	return db_photo_table_move(sp->db, from, to);
}

static int changed_for_each_callback(void *data, int argc, char **argv,
				     char **col_name)
{
	D();

	struct smart_photo *sp = (struct smart_photo *)data;
	const std::string path_col_name = "path";

	for (int i = 0; i < argc; i++) {
		if (path_col_name == col_name[i]) {
			if (argv[i] == NULL)
				continue;

			if (!sp->scan_runing)
				break;

			sp->changed_photos.push_back(std::string(argv[i]));
		}
	}

	return 0;
}


static int infer(cv::Mat img, const char *model,
		 std::vector<std::vector<float>*> &r)
{
	if (img.empty())
		return -1;

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
	int rc = vino_ie_pipeline_infer_image(images,
					      additional_input,
					      std::string(model),
					      r,
					      url_info);
	if (rc != 0)
		return -1;
	return 0;
}

static int face_detection_ex(cv::Mat img,
			     std::function<void (cv::Rect &)> callback)
{
	std::vector<float> detections;
	std::vector<std::vector<float>*> raw_results;
	raw_results.push_back(&detections);

	int rc = infer(img, FACE_DETECTION_MODEL, raw_results);
	if (rc == RT_INFER_ERROR) {
		E("infer error");
		return -1;
	} else if (rc == RT_REMOTE_INFER_OK) {
		// FIXME:
		E("get RT_REMOTE_INFER_OK");
		return -1;
	}
	D("rc=" << rc << " detections.size()=" << detections.size());

	// decode vino(OpenVINO) result
	// the model output object_size is 7, lenght is 200
	const int object_size = 7;
	const int results_size = 200;
	// check if the infer result size is correct.
	if (detections.size() != object_size * results_size) {
		E("incrrect detection.size:" << detections.size());
		return -1;
	}

	int img_w = img.cols;
	int img_h = img.rows;
	D("img_w=" << img_w << ", img_h=" << img_h);
	std::string ie_result;
	for (int i = 0; i < results_size; i++) {
		int offset = i * object_size;
		float image_id = detections[offset];
		if (image_id < 0)
			break;

		// confidence
		if (detections[offset+2] < FACE_DETECTION_CONFIDENCE)
			continue;

		D("confidence=" << detections[offset + 2]);

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
		callback(rect);
	}

	return 0;
}

static bool ignore_face(cv::Mat &face_img)
{
	std::vector<std::vector<float>*> raw_results;
	std::vector<float> yaw;
	std::vector<float> pitch;
	std::vector<float> roll;
	raw_results.push_back(&pitch);
	raw_results.push_back(&roll);
	raw_results.push_back(&yaw);

	int rc = infer(face_img, HEAD_POSE_MODEL, raw_results);
	if (rc != RT_LOCAL_INFER_OK) {
		E("head pose infer failed");
		return true;
	}
	if (yaw.size() != 1 || pitch.size() != 1 || roll.size() != 1) {
		E("head pose result size error");
		return true;
	}

	float y = fabs(yaw[0]);
	float p = fabs(pitch[0]);
	float r = fabs(roll[0]);
	D("head pose abs value y:" << y << " p:" << p << " r:" << r);
	if (y > HEAD_POSE_MAX_ANGLE || p > HEAD_POSE_MAX_ANGLE) {
		D("head pose angle too large, ignore this face.");
		return true;
	}

	return false;
}

static int face_detection(cv::Mat img, std::vector<std::vector<float> >* vs)
{
	if (!vs)
		return -1;

	auto callback = [&] (cv::Rect &rect) {
		int img_w = img.cols;
		int img_h = img.rows;

		std::vector<std::shared_ptr<cv::Mat>> images;
		// clip rect
		auto clipped_size = rect & cv::Rect(0, 0, img_w, img_h);
		D("clip=" << clipped_size);

		// ignore if face size too small
		if ((float)clipped_size.width / (float)img_w < FACE_MIN_PERCENT
		    || (float)clipped_size.height / (float)img_h
		    < FACE_MIN_PERCENT) {
			D("face size too small");
			return;
		}
		// face image
		cv::Mat face_img = cv::Mat(img, clipped_size);
		if (ignore_face(face_img)) {
			D("ignore this face");
			return;
		}

		std::vector<float> detections;
		std::vector<std::vector<float>*> raw_results;
		raw_results.push_back(&detections);

		int rc = infer(face_img,
			       FACE_REIDENTIFICATION_MODEL, raw_results);
		if (rc != RT_LOCAL_INFER_OK) {
			E("FACE_REIDENTIFICATION infer failed");
			return;
		}
		D("FACE_REIDENTIFICATION_MODEL detections.size()="
		  << detections.size());

		(*vs).push_back(detections);
	};

	return face_detection_ex(img, callback);
}

static int read_lable_file(const char *label_file,
			    std::vector<std::string> *labels)
{
	std::ifstream stream;
	stream.open(label_file, std::ios::in);
	if (!stream.is_open()) {
		E("cannot open lable file: " << label_file);
		return -1;
	}
	std::string str_line;
	while (std::getline(stream, str_line)) {
		labels->push_back(str_line);
		D(<< str_line);
	}
	stream.close();

	return 0;
}

static int segmentation(cv::Mat img, std::vector<std::string> *markers)
{
	if (!markers) {
		E("need markers should not be NULL");
		return -1;
	}

	static std::vector<std::string> labels;
	// init labels
	if (labels.size() == 0) {
		if (read_lable_file(SEGMENTATION_LABELS, &labels) != 0) {
			E("read lable file failed");
			return -1;
		}
	}

	std::vector<float> detection;
	std::vector<std::vector<float>*> raw_results;
	raw_results.push_back(&detection);

	int rc = infer(img, SEGMENTATION_MODEL, raw_results);
	if (rc == RT_INFER_ERROR) {
		E("infer error");
		return -1;
	} else if (rc == RT_REMOTE_INFER_OK) {
		// FIXME:
		E("get RT_REMOTE_INFER_OK");
		return -1;
	}
	D("rc=" << rc << " detection.size()=" << detection.size());

	const int c = 150;
	const int h = 320;
	const int w = 320;

	if (detection.size() != c * h * w)
		return -1;

	std::map<int, std::string> marker_ids;
	std::vector<int> marker_counts(c, 0);

	for (int pos = 0; pos < h * w; pos ++) {
		float probability = 0;
		int clss = -1;
		for (int i = 0; i < c; i ++) {
			float val = detection[(i * h * w) + pos];
			if (val > probability) {
				probability = val;
				clss = i;
			}
		}
		if (probability > SEGMENTATION_CONFIDENCE) {
			marker_ids[clss] = labels[clss];
			marker_counts[clss] += 1;
		}

	}
	for (auto c: marker_ids) {
		int count = marker_counts[c.first];
		if (count < h * w * 0.06)
			continue;

		D("id=" << c.first << ", class=" << c.second
		  << ", count=" << marker_counts[c.first]);
		markers->push_back(c.second);
	}

	return 0;
}

int cos_distance(const cv::Mat &descr1, const cv::Mat &descr2, float *dist)
{
	if (dist == NULL)
		return -1;
	if (descr1.empty())
		return -1;
	if (descr2.empty())
		return -1;
	if (descr1.size() != descr2.size())
		return -1;

	double xy = descr1.dot(descr2);
	double xx = descr1.dot(descr1);
	double yy = descr2.dot(descr2);
	double norm = sqrt(xx * yy) + 1e-6;
	*dist =  0.5f * static_cast<float>(1.0 - xy / norm);

	return 0;
}

static int person_for_each_callback(void *data, int argc, char **argv, char
				    **col_name)
{
	struct match_face *mf = (struct match_face *)data;
	const std::string str_face_embeddings = "face_embeddings";
	const std::string str_id = "id";

	int64_t id = 0;
	std::vector<float> v;

	cv::Mat em(512, 1, cv::DataType<float>::type);

	for (int i = 0; i < argc; i++) {
		if (argv[i] == NULL)
			continue;
		if (str_id == col_name[i])
			id = stoll(argv[i]);
		if (str_face_embeddings == col_name[i])
			memcpy(em.data, argv[i], 512*sizeof(float));
	}

	float distance;
	cos_distance(mf->m, em, &distance);

	D("id=" << id);
	D("distance=" << distance);

	if (distance < 0.25)
		mf->ids.push_back(id);

	return 0;
}

static void *scan_thread(void *p)
{
	struct smart_photo *sp = (struct smart_photo *)p;
	if (sp == NULL)
		return NULL;

	smart_photo_lock(sp);

	while (sp->scan_runing) {
		db_changed_table_for_each_row(sp->db,
					      changed_for_each_callback, sp);
		if (sp->changed_photos.size() == 0)
			break;

		db_transaction_begin(sp->db);

		for (auto p : sp->changed_photos) {
			if (!sp->scan_runing)
				break;

			smart_photo_unlock(sp);

			//---------------- infer
			D("infer: " << p);
			cv::Mat img = cv::imread(p);

			if (img.empty()) {
				E("can not load photo: " << p << " remove from db");

				// remove the photo cannot loaded by cv.
				smart_photo_lock(sp);
				db_changed_table_remove(sp->db, p.c_str());

				continue;
			}

			// face detection
			std::vector<std::vector<float> > vs;
			face_detection(img, &vs);

			// segmentation
			std::vector<std::string> labels;
			segmentation(img, &labels);

			//---------------- update db
			smart_photo_lock(sp);


			// remove p from db
			db_changed_table_remove(sp->db, p.c_str());

			// object to db
			int64_t p_id;
			db_photo_table_add(sp->db, p.c_str(), &p_id);

			for (auto &l : labels) {
				D("LABEL: "<< l);
				int64_t id;
				db_object_table_add(sp->db, l.c_str(), &id);
				D("id=" << id);
				db_photo_object_table_add(sp->db, p_id, id);
			}

			// face id to db
			std::vector<std::vector<float> > new_persion_vs;
			for (auto &v : vs) {
				D("v.size()=" << v.size());
				cv::Mat m(v);

				struct match_face mf;
				mf.sp = sp;
				mf.m = m;
				mf.ids.clear();

				db_person_table_for_each_row(sp->db,
							     person_for_each_callback, &mf);

				D("mf.ids.size()=" << mf.ids.size());
				if (mf.ids.size()) {
					//
					D("face id found");
					for (auto id: mf.ids) {
						D("face id=" << id);
						db_photo_person_table_add(sp->db, p_id,
									  id);
					}
				} else {
					new_persion_vs.push_back(v);
				}
			}
			for (auto &v : new_persion_vs) {
				int64_t id = 0;
				db_person_table_add(sp->db, v, &id);
				db_photo_person_table_add(sp->db, p_id, id);
				D("new face id=" << id);
			}
		}

		db_transaction_end(sp->db);
		sp->changed_photos.clear();
	}

	sp->scan_runing = 0;
	D(<< "scan stoped");
	smart_photo_unlock(sp);

	return NULL;
}

int ccai_sp_scan(void *sp_handle)
{
	D();

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;
	smart_photo_auto_lock(sp);
	if (sp->scan_runing)
		return -1;

	sp->scan_runing = 1;
	pthread_create(&sp->scan_thread, NULL, scan_thread, sp_handle);

	return 0;
}

int ccai_sp_stop_scan(void *sp_handle)
{
	D();

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;

	smart_photo_lock(sp);
	if (!sp->scan_runing) {
		smart_photo_unlock(sp);
		return -1;
	}
	sp->scan_runing = 0;
	smart_photo_unlock(sp);

	pthread_join(sp->scan_thread, NULL);

	return 0;
}

int ccai_sp_wait_scan(void *sp_handle)
{
	D();

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	if (sp == NULL)
		return -1;
	smart_photo_lock(sp);
	if (!sp->scan_runing) {
		smart_photo_unlock(sp);
		return -1;
	}
	smart_photo_unlock(sp);

	pthread_join(sp->scan_thread, NULL);

	return 0;
}

static int table_list(void *sp_handle,
		      CCAI_SP_LIST_CB callback,
		      void *data,
		      std::string col_name,
		      DB_LIST_FUNCTION list_func)
{
	struct callback_data {
		CCAI_SP_LIST_CB *callback;
		void *data;
		std::string col_name;
	};

	if (sp_handle == NULL || callback == NULL)
		return -1;

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	smart_photo_auto_lock(sp);

	auto cb = [] (void* data, int argc, char** argv,
			    char** col_name) -> int {

		struct callback_data *cd = (struct callback_data *)data;

		const std::string str_id = "id";
		const std::string str_name = cd->col_name;
		int64_t id = 0;
		char *name = NULL;

		for (int i = 0; i < argc; i++) {
			if (str_id == col_name[i]) {
				id = stoll(argv[i]);
			} else if (str_name == col_name[i]) {
				if (argv[i])
					name = argv[i];
			}
		}

		return cd->callback(cd->data, id, name);
	};

	struct callback_data cd;
	cd.callback = callback;
	cd.data = data;
	cd.col_name = col_name;

	list_func(sp->db, cb, (void *)&cd);

	return 0;
}

int ccai_sp_list_person(void *sp_handle, CCAI_SP_LIST_CB callback,
			void *data)
{
	return table_list(sp_handle, callback, data, "name",
			  db_person_table_list);
}

int ccai_sp_list_class(void *sp_handle, CCAI_SP_LIST_CB callback,
		       void *data)
{
	return table_list(sp_handle, callback, data, "name",
			  db_class_table_list);
}

int ccai_sp_list_photo(void *sp_handle, CCAI_SP_LIST_CB callback,
		       void *data)
{
	return table_list(sp_handle, callback, data, "path",
			  db_photo_table_list);
}

int ccai_sp_list_photo_by_person(void *sp_handle, int64_t id,
				 CCAI_SP_LIST_CB callback, void *data)
{
	struct callback_data {
		CCAI_SP_LIST_CB *callback;
		void *data;
	};

	if (sp_handle == NULL || callback == NULL)
		return -1;

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	smart_photo_auto_lock(sp);

	auto cb = [] (void* data, int argc, char** argv,
			    char** col_name) -> int {

		struct callback_data *cd = (struct callback_data *)data;
		const std::string str_id = "id";
		const std::string str_path = "path";
		int64_t id = 0;
		char *path = NULL;

		for (int i = 0; i < argc; i++) {
			if (str_id == col_name[i]) {
				id = stoll(argv[i]);
			} else if (str_path == col_name[i]) {
				if (argv[i])
					path = argv[i];
			}
		}

		return cd->callback(cd->data, id, path);
	};

	struct callback_data cd;
	cd.callback = callback;
	cd.data = data;

	db_list_photo_by_person(sp->db, cb, (void *)&cd, id);

	return 0;
}

int ccai_sp_list_photo_by_class(void *sp_handle, const char* clss,
				CCAI_SP_LIST_CB callback, void *data)
{
	struct callback_data {
		CCAI_SP_LIST_CB *callback;
		void *data;
	};

	if (sp_handle == NULL || callback == NULL)
		return -1;

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	smart_photo_auto_lock(sp);

	auto cb = [] (void* data, int argc, char** argv,
			    char** col_name) -> int {

		struct callback_data *cd = (struct callback_data *)data;
		const std::string str_id = "id";
		const std::string str_path = "path";
		int64_t id = 0;
		char *path = NULL;

		for (int i = 0; i < argc; i++) {
			if (str_id == col_name[i]) {
				id = stoll(argv[i]);
			} else if (str_path == col_name[i]) {
				if (argv[i])
					path = argv[i];
			}
		}

		return cd->callback(cd->data, id, path);
	};

	struct callback_data cd;
	cd.callback = callback;
	cd.data = data;

	db_list_photo_by_class(sp->db, cb, (void *)&cd, clss);

	return 0;
}

int ccai_sp_count_photo_by_person(void *sp_handle, int64_t id)
{
	int count = 0;

	if (sp_handle == NULL)
		return -1;

	struct smart_photo *sp = (struct smart_photo *)sp_handle;
	smart_photo_auto_lock(sp);

	auto cb = [] (void* data, int, char**, char**) -> int {
		if (data == NULL)
			return -1;

		int *count = (int *)data;
		(*count) ++;

		return 0;
	};

	db_list_photo_by_person(sp->db, cb, (void *)&count, id);

	D("count=" << count);

	return count;
}

int ccai_sp_list_all_class(void *sp_handle, CCAI_SP_LIST_CB callback,
			   void *data)
{
	std::vector<std::string> labels;
	if (read_lable_file(SEGMENTATION_LABELS, &labels) != 0) {
		E("read lable file failed");
		return -1;
	}
	for (auto& l : labels) {
		callback(data, -1, l.c_str());
	}

	return 0;
}

