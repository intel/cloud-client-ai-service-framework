#ifndef CCAI_LOG_H
#define CCAI_LOG_H
#pragma once

#include <stdarg.h>
#include <syslog.h>

#define CCAI_ALERT(...)  ccai_log(LOG_ALERT, __VA_ARGS__)
#define CCAI_CRIT(...)   ccai_log(LOG_CRIT, __VA_ARGS__)
#define CCAI_ERROR(...)  ccai_log(LOG_ERR, __VA_ARGS__)
#define CCAI_WARN(...)   ccai_log(LOG_WARNING, __VA_ARGS__)
#define CCAI_NOTICE(...) ccai_log(LOG_NOTICE, __VA_ARGS__)
#define CCAI_INFO(...)   ccai_log(LOG_INFO, __VA_ARGS__)
#ifndef NDEBUG
#define CCAI_DEBUG(...)  ccai_log(LOG_DEBUG, __VA_ARGS__)
#else
#define CCAI_DEBUG(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void ccai_log(int pri, const char* fmt, ...);
void ccai_vlog(int pri, const char* fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif
