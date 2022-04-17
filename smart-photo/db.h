#ifndef _DB_H
#define _DB_H

#include <sqlite3.h>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

typedef int DB_EXEC_CALLBACK(void*,int,char**,char**);

int db_init(sqlite3 **pdb, const char *db_path);
int db_finit(sqlite3 *db);

int db_transaction_begin(sqlite3 *db);
int db_transaction_end(sqlite3 *db);
int db_changed_table_add(sqlite3 *db, const char *path);
int db_changed_table_remove(sqlite3 *db, const char *path);

int db_changed_table_for_each_row(sqlite3 *db, DB_EXEC_CALLBACK callback,
				  void *data);

int db_photo_table_add(sqlite3 *db, const char *path, int64_t *id);
int db_photo_table_remove(sqlite3 *db, const char *path);
int db_photo_table_move(sqlite3 *db, const char *from, const char *to);

int db_class_table_add(sqlite3 *db, const char *name);

int db_object_table_add(sqlite3 *db, const char *name, int64_t *id);

int db_photo_object_table_add(sqlite3 *db,
			      const int64_t p_id, const int64_t o_id);

int db_person_table_add(sqlite3 *db, const std::vector<float> &v, int64_t *id);
int db_person_table_for_each_row(sqlite3 *db, DB_EXEC_CALLBACK callback,
				 void *data);

int db_photo_person_table_add(sqlite3 *db,
		const int64_t photo_id, const int64_t person_id);

int db_person_table_list(sqlite3 *db, DB_EXEC_CALLBACK callback,
			 void *data);

int db_class_table_list(sqlite3 *db, DB_EXEC_CALLBACK callback,
			void *data);

int db_photo_table_list(sqlite3 *db, DB_EXEC_CALLBACK callback,
			void *data);

int db_list_photo_by_person(sqlite3 *db, DB_EXEC_CALLBACK callback,
			    void *data, int64_t person_id);
int db_list_photo_by_class(sqlite3 *db, DB_EXEC_CALLBACK callback,
			   void *data, const char *class_name);
int db_list_class_by_photo(sqlite3 *db, DB_EXEC_CALLBACK callback,
			   void *data, const char *path);
int db_list_person_by_photo(sqlite3 *db, DB_EXEC_CALLBACK callback,
			    void *data, const char *path);
int db_photo_table_remove_all(sqlite3 *db);
#ifdef __cplusplus
}
#endif
#endif

