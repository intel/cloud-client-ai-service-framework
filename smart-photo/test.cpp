#include <unistd.h>
#include <iostream>
#include <vector>
#include "ccai_smart_photo.h"

#include "smart_photo_log.h"

#define SQLITE3_DB_FILE "/smart-photo/sp.db"

int main(int argc, char** argv)
{
	void *sp = NULL;
	int r;

	r = ccai_sp_init(&sp, SQLITE3_DB_FILE);
	D("ccai_sp_init r=" << r);

// test scan
#if 1
	r = ccai_sp_add_dir(sp, "/smart-photo/photos");
	D("ccai_sp_add_dir r=" << r);

	r = ccai_sp_scan(sp);
	D("ccai_sp_scan r=" << r);

	sleep(1);
	r = ccai_sp_stop_scan(sp);
	D("ccai_sp_stop_scan r=" << r);

	r = ccai_sp_scan(sp);
	D("ccai_sp_scan r=" << r);

#if 0
	sleep(3);
#else
	r = ccai_sp_wait_scan(sp);
	D("ccai_sp_wait_scan r=" << r);
#endif
#endif


	std::vector<int64_t> person_ids;

	// list person
	auto cb = [] (void *data, int64_t id, const char *name) -> int {
		std::vector<int64_t> *person_ids =
			(std::vector<int64_t> *)data;
		if (!person_ids)
			return -1;

		std::string str_name;
		if (name)
			str_name = name;

		D("ccai_sp_list_person id=" << id << ", name=" << str_name);

		person_ids->push_back(id);
		return 0;
	};
	r = ccai_sp_list_person(sp, cb, &person_ids);
	D("ccai_sp_list_person r=" << r);

	for (auto id : person_ids)
		D("person_ids: " << id);;

	// list class
	std::vector<int64_t> ids;
	auto cb_class = [] (void *data, int64_t id, const char *name) -> int {
		std::vector<int64_t> *ids =
			(std::vector<int64_t> *)data;
		if (!ids)
			return -1;

		std::string str_name;
		if (name)
			str_name = name;

		D("ccai_sp_list_person id=" << id << ", name=" << str_name);

		ids->push_back(id);
		return 0;
	};
	r = ccai_sp_list_class(sp, cb_class, &ids);
	D("ccai_sp_list_class r=" << r);

	for (auto id : ids)
		D("ids: " << id);;

	// list by person
	std::vector<std::string> photo_paths;
	auto cb_pp = [] (void *data, int64_t id, const char *path) -> int {
		std::vector<std::string> *photo_paths =
			(std::vector<std::string> *)data;
		if (!photo_paths)
			return -1;

		std::string str_path;
		if (path)
			str_path = path;

		D("ccai_sp_list_photo_by_... id=" << id
		  << ", path=" << str_path);

		photo_paths->push_back(str_path);
		return 0;
	};

	r = ccai_sp_list_photo_by_person(sp, 1, cb_pp, &photo_paths);

	D("ccai_sp_list_photo_by_person r=" << r);

	// list by class
	photo_paths.clear();
	r = ccai_sp_list_photo_by_class(sp, "cat", cb_pp, &photo_paths);

	photo_paths.clear();
	r = ccai_sp_list_photo_by_class(sp, "person", cb_pp, &photo_paths);

	D("ccai_sp_list_photo_by_class r=" << r);


	// list photo
	photo_paths.clear();
	D("list all photo");
	r = ccai_sp_list_photo(sp, cb_pp, &photo_paths);
	D("ccai_sp_list_photo r=" << r);

	D("photo count");
	r = ccai_sp_count_photo_by_person(sp, 2);
	D("ccai_sp_count_photo_by_person r=" << r);

	// release
	r = ccai_sp_release(sp);
	D("ccai_sp_release r=" << r);

	sp = NULL;

	return 0;
}
