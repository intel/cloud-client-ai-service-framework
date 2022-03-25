#ifndef _SMART_PHOTO_H
#define _SMART_PHOTO_H

#include <pthread.h>
#include <vector>
#include <mutex>

#include "db.h"

#define SP_SCAN_STARTED  1
#define SP_SCAN_STOPPING 2
#define SP_SCAN_STOPED   3

struct smart_photo {
	sqlite3 *db;
	std::vector<std::string> changed_photos;
	std::mutex mutex;
	pthread_t scan_thread;
	int scan_flag;

	smart_photo() {
		scan_flag = SP_SCAN_STOPED;
	}
};

#define smart_photo_auto_lock(sp) std::lock_guard<std::mutex> guard((sp)->mutex)
#define smart_photo_lock(sp) (sp)->mutex.lock()
#define smart_photo_unlock(sp) (sp)->mutex.unlock()


#endif
