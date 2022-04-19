#include <string.h>
#include <assert.h>
#include <iostream>
#include <vector>
#include "ccai_smart_photo.h"

#include "smart_photo_log.h"
#include "db.h"


static int init_tables(sqlite3 *db)
{
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	const char *sql =
		"PRAGMA foreign_keys = ON;"
		"CREATE TABLE IF NOT EXISTS ChangedPhoto ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT);"
		"CREATE UNIQUE INDEX IF NOT EXISTS "
			"ChangedPhotoPathIndex ON ChangedPhoto (path);"
		"CREATE TABLE IF NOT EXISTS Photo ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT);"
		"CREATE UNIQUE INDEX IF NOT EXISTS "
			"PhotoPathIndex ON Photo (path);"
		""
		"CREATE TABLE IF NOT EXISTS Classification ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT);"
		"CREATE UNIQUE INDEX IF NOT EXISTS "
			"ClassNameIndex ON Classification (name);"
		"CREATE TABLE IF NOT EXISTS PhotoClass ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"photo_id INTEGER,"
			"class_id INTEGER,"
			"FOREIGN KEY(photo_id) REFERENCES photo(id),"
			"FOREIGN KEY(class_id) REFERENCES classification(id));"
		""
		"CREATE TABLE IF NOT EXISTS Object ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT);"
		"CREATE UNIQUE INDEX IF NOT EXISTS "
			"ObjectNameIndex ON Object (name);"
		"CREATE TABLE IF NOT EXISTS PhotoObject ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"photo_id INTEGER,"
			"object_id INTEGER,"
			"FOREIGN KEY(photo_id) REFERENCES photo(id) ON DELETE CASCADE,"
			"FOREIGN KEY(object_id) REFERENCES object(id) ON DELETE CASCADE);"
		"CREATE TABLE IF NOT EXISTS Person ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT,"
			"face_embeddings BLOB);"
		"CREATE TABLE IF NOT EXISTS PhotoPerson ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"photo_id INTEGER,"
			"person_id INTEGER,"
			"FOREIGN KEY(photo_id) REFERENCES photo(id) ON DELETE CASCADE,"
			"FOREIGN KEY(person_id) REFERENCES person(id) ON DELETE CASCADE);";

	int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		D(<< "create table ChangedPhoto failed: " << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_init(sqlite3 **pdb, const char *db_path)
{
	if (pdb == NULL)
		return -1;

	D(<< "sqlite version:" << sqlite3_libversion());
	int rc = sqlite3_open(db_path, pdb);
	if (rc != SQLITE_OK) {
		D(<< "cannot open db:" << db_path);
		return -1;
	}

	rc = init_tables(*pdb);
	if (rc != 0)
		return -1;

	return 0;
}

int db_finit(sqlite3 *db)
{
	D();

	if (db == NULL)
		return -1;

	int rc = sqlite3_close(db);
	if (rc != SQLITE_OK)
		return -1;

	return 0;
}

int db_transaction_begin(sqlite3 *db)
{
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	int rc = sqlite3_exec(db, "BEGIN;", 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		D(<< "db transcation begin failed: " << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}
	return 0;
}

int db_transaction_end(sqlite3 *db)
{
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	int rc = sqlite3_exec(db, "END;", 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		D(<< "db transcation end failed: " << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}
	return 0;
}

int db_changed_table_add(sqlite3 *db, const char *path)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"INSERT INTO ChangedPhoto (path) "
		"VALUES ('";
	sql += path;
	sql += "');";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		if (rc == SQLITE_CONSTRAINT) {
			D(<< "ignore UNIQUE index constraint");
			return 0;
		}
		E(<< "ChangedPhoto insert failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_changed_table_remove(sqlite3 *db, const char *path)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"DELETE FROM ChangedPhoto WHERE path = '";
	sql += path;
	sql += "';";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		D(<< path);
		D(<< "ChangedPhoto delete failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

//int db_changed_table_for_each_row(sqlite3 *db,
		//int (*callback)(void*,int,char**,char**), void *data)

int db_changed_table_for_each_row(sqlite3 *db, DB_EXEC_CALLBACK callback,
				  void *data)
{
	D();

	if (db == NULL)
		return -1;

	const char *sql = "SELECT path FROM ChangedPhoto;";
	char *err_msg = NULL;

	int rc = sqlite3_exec(db, sql, callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		if (rc == SQLITE_ABORT) {
			D(<< "ignore user callback abort select exec");
			return 0;
		}
		D(<< "ChangedPhoto select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}
	return 0;
}

int db_photo_table_add(sqlite3 *db, const char *path, int64_t *id)
{
	D();

	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql = "PRAGMA foreign_keys=ON; DELETE FROM Photo WHERE path='";
	sql += path;
	sql += "';";
	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Photo delete failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	sql = "INSERT INTO Photo (path) VALUES ('";
	sql += path;
	sql += "');";

	rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Photo insert failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	if (!id)
		return 0;

	*id = sqlite3_last_insert_rowid(db);
	D("id=" << *id);

	return 0;
}

int db_class_table_add(sqlite3 *db, const char *name)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"INSERT OR IGNORE INTO Classification (name) "
		"VALUES ('";
	sql += name;
	sql += "');";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		if (rc == SQLITE_CONSTRAINT) {
			D(<< "ignore UNIQUE index constraint");
			return 0;
		}
		E(<< "Classification insert failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_object_table_add(sqlite3 *db, const char *name, int64_t *id)
{
	D();

	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"INSERT OR IGNORE INTO Object (name) "
		"VALUES ('";
	sql += name;
	sql += "');";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Object insert failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	if (!id)
		return 0;

	sql = "SELECT id FROM Object WHERE name ='";
	sql += name;
	sql += "';";

	auto callback = [](void *data,
			   int argc, char** argv, char** col)->int {
		int64_t *id = static_cast<int64_t *>(data);
		if (!id)
			return -1;
		const std::string col_name = "id";
		for (int i = 0; i < argc; i++) {
			if (col_name == col[i]) {
				if (argv[i] == NULL)
					continue;
				D("argv[" << i << "]=" << argv[i]);
				*id = stoll(argv[i]);
			}
		}
		return 0;
	};

	rc = sqlite3_exec(db, sql.c_str(), callback, id, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Object select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	D("id=" << *id);

	return 0;
}

int db_photo_object_table_add(sqlite3 *db,
			      const int64_t p_id, const int64_t o_id)
{
	D();

	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"INSERT OR IGNORE INTO PhotoObject (photo_id, object_id) "
		"VALUES (";
	sql += to_string(p_id);
	sql += ",";
	sql += to_string(o_id);
	sql += ");";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Object insert failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_person_table_add(sqlite3 *db, const std::vector<float> &v, int64_t *id)
{
	D();

	int rc;
	if (db == NULL)
		return -1;

	std::string insert_sql =
		"INSERT INTO Person (face_embeddings) VALUES (?)";

	sqlite3_stmt *insert_stmt;
	if ((rc = sqlite3_prepare_v2(db, insert_sql.c_str(), -1, &insert_stmt,
				     NULL)) != SQLITE_OK) {
		E("Can't prepare insert statment " << insert_sql << " ("
		  << rc << ") " << sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_bind_blob(insert_stmt, 1, v.data(), v.size() * sizeof(float),
			  SQLITE_STATIC);
	if (SQLITE_DONE != (rc = sqlite3_step(insert_stmt))) {
		E("Insert statement didn't work (" << rc << ") " <<
		  sqlite3_errmsg(db));
		return -1;
	}

	sqlite3_finalize(insert_stmt);

	if (id)
		*id = sqlite3_last_insert_rowid(db);

	return 0;
}

int db_person_table_for_each_row(sqlite3 *db, DB_EXEC_CALLBACK callback,
				 void *data)
{
	D();

	if (db == NULL)
		return -1;

	const char *sql = "SELECT id, face_embeddings FROM Person;";
	char *err_msg = NULL;

	int rc = sqlite3_exec(db, sql, callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		if (rc == SQLITE_ABORT) {
			D(<< "ignore user callback abort select exec");
			return 0;
		}
		D(<< "Person select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}
	return 0;
}


int db_photo_person_table_add(sqlite3 *db, const int64_t photo_id,
			      const int64_t person_id)
{
	D();

	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"INSERT OR IGNORE INTO PhotoPerson (photo_id, person_id) "
		"VALUES (";
	sql += to_string(photo_id);
	sql += ",";
	sql += to_string(person_id);
	sql += ");";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "PhotoPerson insert failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_person_table_list(sqlite3 *db, DB_EXEC_CALLBACK callback, void *data)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	const char *sql = "SELECT id, name FROM Person;";

	int rc = sqlite3_exec(db, sql, callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Person select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_class_table_list(sqlite3 *db, DB_EXEC_CALLBACK callback, void *data)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	const char *sql = "SELECT id, name FROM Object;";

	int rc = sqlite3_exec(db, sql, callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Object select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_photo_table_list(sqlite3 *db, DB_EXEC_CALLBACK callback, void *data)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	const char *sql = "SELECT id, path FROM Photo;";

	int rc = sqlite3_exec(db, sql, callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Photo select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_list_photo_by_person(sqlite3 *db, DB_EXEC_CALLBACK callback,
			    void *data, int64_t person_id)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql = "SELECT p.id, p.path FROM Photo p "
			  "INNER JOIN PhotoPerson pp ON p.id = pp.photo_id "
			  "INNER JOIN Person ps ON ps.id = pp.person_id "
			  "WHERE ps.id = ";
	sql += to_string(person_id);
	sql += ";";

	int rc = sqlite3_exec(db, sql.c_str(), callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Photo by person select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_list_photo_by_class(sqlite3 *db, DB_EXEC_CALLBACK callback,
			   void *data, const char *class_name)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql = "SELECT p.id, p.path FROM Photo p "
			  "INNER JOIN PhotoObject po ON p.id = po.photo_id "
			  "INNER JOIN Object o ON o.id = po.object_id "
			  "WHERE o.name = '";
	sql += class_name;
	sql += "';";

	int rc = sqlite3_exec(db, sql.c_str(), callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "Photo by class select failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}


int db_photo_table_remove(sqlite3 *db, const char *path)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"PRAGMA foreign_keys=ON; DELETE FROM Photo WHERE path = '";
	sql += path;
	sql += "';";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		D(<< path);
		D(<< "Photo delete failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_photo_table_move(sqlite3 *db, const char *from, const char *to)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql = "UPDATE Photo SET path = '";
	sql += to;
	sql += "' WHERE path='";
	sql += from;
	sql += "';";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		D("sql: " << sql);
		D(<< "Photo delete failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_list_class_by_photo(sqlite3 *db, DB_EXEC_CALLBACK callback, void *data,
			   const char *path)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql = "SELECT o.id, o.name FROM Object o "
			  "INNER JOIN PhotoObject po ON o.id = po.object_id "
			  "INNER JOIN Photo p ON p.id = po.photo_id "
			  "WHERE p.path = '";
	sql += path;
	sql += "';";

	int rc = sqlite3_exec(db, sql.c_str(), callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "list class by photo select failed rc: " << rc
		  << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_list_person_by_photo(sqlite3 *db, DB_EXEC_CALLBACK callback,
			    void *data, const char *path)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql = "SELECT ps.id, ps.name FROM Person ps "
			  "INNER JOIN PhotoPerson pp ON ps.id = pp.person_id "
			  "INNER JOIN Photo p ON p.id = pp.photo_id "
			  "WHERE p.path = '";
	sql += path;
	sql += "';";

	int rc = sqlite3_exec(db, sql.c_str(), callback, data, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "list person by photo select failed rc: " << rc
		  << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int db_photo_table_remove_all(sqlite3 *db)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql =
		"PRAGMA foreign_keys=ON;"
		"DELETE FROM ChangedPhoto;"
		"DELETE FROM Photo;"
		"DELETE FROM Object;"
		"DELETE FROM PhotoObject;"
		"DELETE FROM Person;"
		"DELETE FROM PhotoPerson;";

	int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		D(<< "remove all failed rc: " << rc << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return 0;
}

int64_t db_changed_table_count(sqlite3 *db)
{
	D();
	if (db == NULL)
		return -1;

	char *err_msg = NULL;
	std::string sql = "SELECT COUNT(*) from ChangedPhoto;";

	auto cb = [] (void* data, int argc, char** argv,
			    char** col_name) -> int {
		D("argc=" << argc);
		int64_t *c = (int64_t *)data;
		if (argc != 1)
			return -1;
		*c = stoll(argv[0]);
		return 0;
	};

	int64_t count = -1;

	int rc = sqlite3_exec(db, sql.c_str(), cb, &count, &err_msg);
	if (rc != SQLITE_OK) {
		E(<< "changed table count failed rc: " << rc
		  << " err_msg: "
		  << err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	return count;
}

