#ifndef CCAI_SMART_PHOTO_H
#define CCAI_SMART_PHOTO_H
#ifdef __cplusplus
extern "C" {
#endif

#define CCAI_SP_NAME_SIZE_MAX	512

typedef int CCAI_SP_LIST_CB(void *, int64_t, const char*);

int ccai_sp_init(void **sp_handle, const char *db_path);
int ccai_sp_release(void *sp_handle);
int ccai_sp_add_dir(void *sp_handle, const char *dir);
int ccai_sp_add_file(void *sp_handle, const char *file);
int ccai_sp_remove_file(void *sp_handle, const char *file);
int ccai_sp_remove_all_file(void *sp_handle);
int ccai_sp_move_file(void *sp_handle, const char *from, const char *to);

int ccai_sp_scan(void *sp_handle);
int ccai_sp_wait_scan(void *sp_handle);
int ccai_sp_stop_scan(void *sp_handle);

int ccai_sp_list_person(void *sp_handle, CCAI_SP_LIST_CB callback,
			void *data);
int ccai_sp_list_class(void *sp_handle, CCAI_SP_LIST_CB callback,
		       void *data);
int ccai_sp_list_photo(void *sp_handle, CCAI_SP_LIST_CB callback,
		       void *data);
int ccai_sp_list_photo_by_person(void *sp_handle, int64_t id,
				 CCAI_SP_LIST_CB callback,  void *data);
int ccai_sp_list_photo_by_class(void *sp_handle, const char* clss,
				CCAI_SP_LIST_CB, void *data);
int ccai_sp_count_photo_by_person(void *sp_handle, int64_t id);

int ccai_sp_list_all_class(void *sp_handle, CCAI_SP_LIST_CB callback,
			   void *data);

int ccai_sp_scan_file_buffer(void *sp_handle, const char *path,
			     const char *buffer, int len);

int ccai_sp_list_class_by_photo(void *sp_handle, const char *path,
				CCAI_SP_LIST_CB callback, void *data);

int ccai_sp_list_person_by_photo(void *sp_handle, const char *path,
				 CCAI_SP_LIST_CB callback, void *data);

int ccai_sp_scan_running(void *sp_handle);
int64_t ccai_sp_waiting_scan_count(void *sp_handle);

#ifdef __cplusplus
}
#endif
#endif
