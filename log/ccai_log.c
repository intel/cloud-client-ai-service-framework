#include <stdarg.h>
#include <string.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <unistd.h>

#include "log.h"

static log_t *log;
static int mask;

__attribute__((weak,visibility("default")))
const char* ccai_log_ident     = NULL;
__attribute__((weak,visibility("default")))
const char* ccai_log_stderr    = NULL;
__attribute__((weak,visibility("default")))
const char* ccai_log_server    = NULL;
__attribute__((weak,visibility("default")))
const char* ccai_log_threshold = NULL;

static void __attribute__ ((constructor)) ccai_log_init(void);
static void __attribute__ ((destructor)) ccai_log_fini(void);

void ccai_log_init(void) {
    const char *str;
    str = getenv("CCAI_LOG_THRESHOLD") ?: ccai_log_threshold;
    mask = LOG_UPTO(LOG_DEBUG);
    if (str) {
        for (int i = 0; prioritynames[i].c_name; ++i) {
            if (!strcasecmp(str, prioritynames[i].c_name)) {
                mask = LOG_UPTO(prioritynames[i].c_val);
                setlogmask(mask);
                break;
            }
        }
    }

    int opt = LOG_PID;
    str = getenv("CCAI_LOG_STDERR") ?: ccai_log_stderr;
#ifdef NDEBUG
    if (str && (!strcasecmp(str, "y") || !strcmp(str, "1")))
#else
    if (!str || (str[0] && strcasecmp(str, "n") && strcmp(str, "0")))
#endif
        opt |= LOG_PERROR;

    str = getenv("CCAI_LOG_IDENT") ?: ccai_log_ident;
    log = log_new(getenv("CCAI_LOG_SERVER") ?: ccai_log_server);
    if (log) {
        if (str)
            log_setapp(log, 0, str);
        if (opt & LOG_PERROR)
            log_setoutput(log, STDERR_FILENO);
    } else {
        openlog(str, opt, LOG_USER);
    }
}

__attribute__ ((visibility("default")))
void ccai_vlog(int pri, const char* fmt, va_list ap) {
    if (!log)
        vsyslog(pri, fmt, ap);
    else if (LOG_MASK(LOG_PRI(pri)) & mask)
        log_vsend(log, pri, fmt, ap);
}

__attribute__ ((visibility("default")))
void ccai_log(int pri, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (!log)
        vsyslog(pri, fmt, ap);
    else if (LOG_MASK(LOG_PRI(pri)) & mask)
        log_vsend(log, pri, fmt, ap);
    va_end(ap);
}

void ccai_log_fini(void) {
    if (log) {
        log_free(log);
        log = NULL;
    } else {
        closelog();
    }
}
