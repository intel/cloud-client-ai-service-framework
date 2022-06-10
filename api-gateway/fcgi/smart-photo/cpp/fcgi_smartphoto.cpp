// Copyright (C) 2020 Intel Corporation

/*
API example
=======
curl -d '{
"method":"list_class",
}' -H "Content-Type: application/json" -X POST http://localhost:8080/cgi-bin/smartphoto


*/

#include <string>
#include <stdio.h>
#include <sys/time.h>

#include <fcgiapp.h>
#include <fcgio.h>
#include <json-c/json.h>

#include <ccai_smart_photo.h>
#include "smart_photo_log.h"

#include "../../utils/cpp/fcgi_utils.h"

#define DEFAULT_SMART_PHOTO_DIR		"/smartphoto/"


static char _smartphoto_db_file[PATH_MAX];

void init_smartphoto_dir()
{
	char *d = getenv("CCAI_SMARTPHOTO_DIR");
	if (d != NULL) {
		snprintf(_smartphoto_db_file, sizeof(_smartphoto_db_file),
			 "%s/%s", d, ".smartphoto.db");
	} else {
		snprintf(_smartphoto_db_file, sizeof(_smartphoto_db_file),
			 DEFAULT_SMART_PHOTO_DIR"/%s", ".smartphoto.db");
	}
}

std::string smartphoto_resp(const char *post_data, void *sp)
{
	D("post_data: " << post_data);

	std::string resp_str = "{\"result\" : -1}";

	struct json_object *resp =  json_object_new_object();
	if (resp == NULL) {
		E("new json_object failed");
		return resp_str;
	}

	struct json_object *req = NULL;
	if ((req = json_tokener_parse(post_data)) == NULL) {
		E("json_tokener_parse error");
		return resp_str;
	}

	std::string method;
	struct json_object *req_param = NULL;

	json_object_object_foreach(req, key, value) {
		if (strncmp(key, "method", 6) == 0) {
			method = json_object_get_string(value);
		} else if (strncmp(key, "param", 4) == 0) {
			req_param = value;
		} else {
			E("Unknow smartphoto request key: " << key);
		}
	}

	D("method=" << method);
	if (method == "add_dir") {
		if (req_param == NULL) {
			E("add_dir need param");
			goto out;
		}
		const char *path = json_object_get_string(req_param);
		int r = ccai_sp_add_dir(sp, path);
		D("ccai_sp_add_dir r=" << r << ", dir=" << path);
		json_object_object_add(resp, "result", json_object_new_int(r));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "scan_start") {
		int  r = ccai_sp_scan(sp);
		D("ccai_sp_scan r=" << r);
		json_object_object_add(resp, "result", json_object_new_int(r));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "scan_running") {
		int scan_running = ccai_sp_scan_running(sp);
		D("ccai_sp_scan_running=" << scan_running);
		json_object_object_add(resp, "result", json_object_new_int(0));
		json_object_object_add(resp, "running",
				       json_object_new_boolean(scan_running));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "scan_stop") {
		int r = ccai_sp_stop_scan(sp);
		json_object_object_add(resp, "result", json_object_new_int(r));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "waiting_scan_count") {
		int count = ccai_sp_waiting_scan_count(sp);
		D("waiting_scan_count=" << count);
		int r = count < 0 ? -1 : 0;
		json_object_object_add(resp, "result", json_object_new_int(r));
		json_object_object_add(resp, "count",
				       json_object_new_int(count));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "list_all_class") {
		// list class
		struct json_object *clsses = json_object_new_array();
		if (clsses == NULL) {
			E("json_object_new_array failed");
			goto out;
		}
		json_object_object_add(resp, "clsses", clsses);
		auto cb_clss = [] (void *data, int64_t id, const char *name) -> int {
			struct json_object *clsses = (struct json_object *)data;
			if (clsses == NULL)
				return -1;
			D("ccai_sp_list_all_class  name="
			  << (name == NULL ? "null" : name));
			json_object_array_add(clsses,
					      json_object_new_string(name));
			return 0;
		};

		int r = ccai_sp_list_all_class(sp, cb_clss, clsses);
		if (r != 0) {
			E("ccai_sp_list_all_class error: " << r);
			goto out;
		}

		D("all clsses: " << json_object_to_json_string(clsses));
		json_object_object_add(resp, "result", json_object_new_int(0));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "list_class") {
		// list class
		struct json_object *clsses = json_object_new_array();
		if (clsses == NULL) {
			E("json_object_new_array failed");
			goto out;
		}
		json_object_object_add(resp, "clsses", clsses);
		auto cb_clss = [] (void *data, int64_t id, const char *name) -> int {
			struct json_object *clsses = (struct json_object *)data;
			if (clsses == NULL)
				return -1;

			D("ccai_sp_list_class id=" << id << ", name="
			  << (name == NULL ? "null" : name));
			struct json_object *c = json_object_new_object();
			if (c == NULL) {
				E("json_object_new_object failed");
				return -1;
			}

			json_object_object_add(c, "id",
					       json_object_new_int64(id));
			json_object_object_add(c, "name",
					       json_object_new_string(name));
			json_object_array_add(clsses, c);

			return 0;
		};

		int r = ccai_sp_list_class(sp, cb_clss, clsses);
		if (r != 0) {
			E("ccai_sp_list_class error: " << r);
			goto out;
		}

		D("clsses: " << json_object_to_json_string(clsses));
		json_object_object_add(resp, "result", json_object_new_int(0));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "list_person") {
		struct person_data {
			int id;
			std::string name;
		};
		std::vector<struct person_data> pds;

		auto cb_person = [] (void *data, int64_t id, const char *name)
			-> int {
			if (data == NULL) return -1;

			std::vector<struct person_data> *pds =
				(std::vector<struct person_data> *)data;

			D("ccai_sp_list_person id=" << id << ", name="
			  << (name == NULL ? "null" : name));

			struct person_data pd;
			pd.id = id;
			if (name)
				pd.name = name;

			pds->push_back(pd);

			return 0;
		};

		int r = ccai_sp_list_person(sp, cb_person, &pds);
		if (r != 0) {
			E("ccai_sp_list_person error: " << r);
			goto out;
		}

		// list person
		struct json_object *person = json_object_new_array();
		if (person == NULL) {
			E("json_object_new_array failed");
			goto out;
		}

		json_object_object_add(resp, "person", person);

		for (auto &pd : pds) {
			struct json_object *c = json_object_new_object();
			if (c == NULL) {
				E("json_object_new_object failed");
				goto out;
			}
			json_object_object_add(c, "id",
				json_object_new_int64(pd.id));
			if (!pd.name.empty())
				json_object_object_add(c, "name",
					json_object_new_string(
						pd.name.c_str()));

			int cnt = ccai_sp_count_photo_by_person(sp, pd.id);
			json_object_object_add(c, "count",
					       json_object_new_int(cnt));

			json_object_array_add(person, c);
		}

		D("clsses: " << json_object_to_json_string(person));
		json_object_object_add(resp, "result", json_object_new_int(0));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "list_photo_by_class" ||
		   method == "list_photo_by_person" ||
		   method == "list_all_photo") {
		// list photo
		struct json_object *photos = json_object_new_array();
		if (photos == NULL) {
			E("json_object_new_array failed");
			goto out;
		}
		json_object_object_add(resp, "photos", photos);

		auto cb_pp = [] (void *data, int64_t id, const char *path) -> int {
			struct json_object *photos = (struct json_object *)data;
			if (photos == NULL)
				return -1;

			D("ccai_sp_list_photo_by_... id=" << id
			  << ", path=" << path);

			struct json_object *p = json_object_new_object();
			if (p == NULL) {
				E("json_object_new_object failed");
				return -1;
			}
			json_object_object_add(p, "id",
					       json_object_new_int64(id));
			if (path)
				json_object_object_add(p, "path",
					       json_object_new_string(path));
			json_object_array_add(photos, p);
			return 0;
		};

		if (req_param == NULL && method != "list_all_photo") {
			E("list_photo_by_class need req_param");
			goto out;
		}

		if (method == "list_photo_by_class") {
			const char *name = json_object_get_string(req_param);
			int r = ccai_sp_list_photo_by_class(sp, name, cb_pp,
							    photos);
			if (r != 0) {
				E("ccai_sp_list_photo_by_class error: " << r);
				goto out;
			}
		} else if (method == "list_photo_by_person") {
			const int64_t id = json_object_get_int64(req_param);
			int r = ccai_sp_list_photo_by_person(sp, id, cb_pp,
							     photos);
			if (r != 0) {
				E("ccai_sp_list_photo_by_person error: " << r);
				goto out;
			}
		} else if (method == "list_all_photo") {
			int r = ccai_sp_list_photo(sp, cb_pp, photos);
			if (r != 0) {
				E("ccai_sp_list_photo error: " << r);
				goto out;
			}
		}

		D("photos: " << json_object_to_json_string(photos));
		json_object_object_add(resp, "result", json_object_new_int(0));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "add_file") {
		D("add_file");
		if (req_param == NULL) {
			E("add_file need req_param");
			goto out;
		}

		std::string path = json_object_get_string(req_param);
		D("file path=" << path);

		int r = ccai_sp_add_file(sp, path.c_str());
		D("ccai_sp_add_file r=" << r);
		if (r == 0) {
			r = ccai_sp_scan(sp);
			D("ccai_sp_scan r=" << r);
			json_object_object_add(resp, "result",
					       json_object_new_int(r));
		} else {
			json_object_object_add(resp, "result",
					       json_object_new_int(r));
		}
		resp_str = json_object_to_json_string(resp);
	} else if (method == "del_file") {
		D("del_file");
		if (req_param == NULL) {
			E("del_file need req_param");
			goto out;
		}

		std::string path = json_object_get_string(req_param);
		D("file path=" << path);

		int r = ccai_sp_remove_file(sp, path.c_str());
		D("ccai_sp_del_file r=" << r);
		json_object_object_add(resp, "result", json_object_new_int(r));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "mv_file") {
		D("mv_file");
		if (req_param == NULL) {
			E("mv_file need req_param");
			goto out;
		}

		struct json_object *from = NULL;
		struct json_object *to = NULL;
		if ((! json_object_object_get_ex(req_param, "from", &from)) ||
		    (! json_object_object_get_ex(req_param, "to", &to))) {
			E("cannot get from,to from mv_file param");
			goto out;
		}

		std::string from_file = json_object_get_string(from);
		std::string to_file = json_object_get_string(to);
		D("from=" << from_file);
		D("to=" << to_file);

		int r = ccai_sp_move_file(sp, from_file.c_str(),
					  to_file.c_str());
		D("ccai_sp_move_file r=" << r);
		json_object_object_add(resp, "result", json_object_new_int(r));
		resp_str = json_object_to_json_string(resp);
	} else if (method == "del_all_file") {
		int r = ccai_sp_remove_all_file(sp);
		json_object_object_add(resp, "result", json_object_new_int(r));
		resp_str = json_object_to_json_string(resp);
	} else {
		E("Unknow method: " << method);
	}

out:
	if (req)
		json_object_put(req);
	if (resp)
		json_object_put(resp);

	return resp_str;
}

int main(int argc, char **argv)
{
	int err;

	init_smartphoto_dir();

	if ((err = FCGX_Init()) != 0) {
		E("FCGX_Init failed: " << err);
		return -1;
	}

	FCGX_Request cgi;
	if ((err = FCGX_InitRequest(&cgi, 0, 0)) != 0) {
		E("FCGX_InitRequest failed: " << err);
		return -2;
	}

	void *sp = NULL;
	if ((err = ccai_sp_init(&sp, _smartphoto_db_file)) != 0) {
		E("ccai_sp_init failed: " << err);
		return -3;
	}


	D(<< "fcgi smartphoto FCGX_Init OK");
	const std::string CORS =
		"Access-Control-Allow-Origin: *\r\n"
		"Access-Control-Allow-Methods: DELETE, POST, GET, OPTIONS\r\n"
		"Access-Control-Allow-Headers: Content-Type, "
		"Access-Control-Allow-Headers, Authorization, "
		"X-Requested-With\r\n";

	while (1) {
		err = FCGX_Accept_r(&cgi);
		if (err) {
			E("FCGX_Accept_r stopped: " << err);
			break;
		}

		// get the post_data form fcgi
		char *post_data = NULL;
		char *lenstr = FCGX_GetParam("CONTENT_LENGTH", cgi.envp);
		unsigned long data_length = 1;

		if (lenstr == NULL ||
		    (data_length += strtoul(lenstr, NULL, 10)) > INT32_MAX) {
			E("get length error");
			std::string result(CORS + "Status: 404 error\r\n"
					   "Content-Type: text/html\r\n\r\n"
					   "get content length error");
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
			continue;
		}

		post_data = (char *)malloc(data_length);
		if (post_data == NULL) {
			E("malloc buffer error");
			std::string result(CORS + "Status: 404 error\r\n"
					   "Content-Type: text/html\r\n\r\n"
					   "malloc buffer error");
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
			continue;
		}
		int ret = FCGX_GetStr(post_data, data_length, cgi.in);
		post_data[ret] = '\0';

		std::string result(CORS + "Status: 200 OK\r\n"
				   "Content-Type: application/json\r\n"
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
			D("post_str.length() = " << post_str.length());
			std::string ret = smartphoto_resp(post_data, sp);
			result += ret;
			FCGX_PutStr(result.c_str(), result.length(), cgi.out);
		}
		// free memory
		free(post_data);
	}

	if (sp)
		ccai_sp_release(sp);

	return 0;
}
