// Copyright (C) 2020 Intel Corporation

#ifndef SERVICE_RUNTIME_HEALTH_MONITOR_H
#define SERVICE_RUNTIME_HEALTH_MONITOR_H

#ifndef DEBUG
#define DEBUG 0
#endif

#define log_debug(fmt, ...) do { if (DEBUG) \
		fprintf(stdout, "AI-Service-Framework %s: " fmt, __func__, ##__VA_ARGS__); \
		} while (0)
#define log_info(fmt, ...) do { fprintf(stdout, "AI-Service-Framework " fmt, ##__VA_ARGS__); } while (0)
#define log_error(fmt, ...) do { \
		fprintf(stderr, "AI-Service-Framework " fmt, ##__VA_ARGS__); } while (0)


#endif
