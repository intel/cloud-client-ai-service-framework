#ifndef __LOG_H
#define __LOG_H

#include <sys/syslog.h>
#include <sys/types.h>
#include <alloca.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct log_t log_t;

/* allocate log contol struct in stack */
#define log_start(server)  log_init(alloca(log_size()), (server))
#define log_end(log)       log_fini(log)

/* allocate log contol struct in heap */
#define log_new(server)    log_init(malloc(log_size()), (server))
#define log_free(log)      { log_fini(log); free(log); }

/* connect to syslog server
 * local server: unix://[/]path/to/socket
 *           or: /path/to/socket
 * remote tcp server: tcp://hostname[:port]
 * remote udp server: udp://hostname[:port]
 * if server is NULL, default local server (/dev/log) is used.
 * if server is hostname:[port], will try udp firstly and then tcp.
 * default port is "syslog", see /etc/services
*/
log_t* log_init(void* buf, const char* server);

/* disconnect server and free all resource */
void log_fini(log_t* log);

/* memory size of log control structure */
size_t log_size();

/* send log message to server */
void log_send(log_t* log, int pri, const char* fmt, ...);
void log_vsend(log_t* log, int pri, const char* fmt, va_list ap);

/* duplicate syslog message to file (usually stderr) */
void log_setoutput(log_t* log, int fd);

/* set facility code (see <sys/syslog.h> */
void log_setfacility(log_t* log, int fac);

/* set process id and name
 * if name is NULL, use the program name.
 * pid must be 0 or self pid if no root permission. */
int log_setapp(log_t* log, pid_t pid, const char* app);

/* set hostname
 * no effective for local server.
 * if hostname is NULL, use system configured hostname (or ip address) */
void log_sethostname(log_t* log, const char* host);

/* set message components defined in RFC5424
 * if all components are 0 or NULL, messages are RFC3164 compliance */
void log_rfc5424opts(log_t* log, int time, int tq, int host, const char* msgid);


#ifdef __cplusplus
}
#endif

#endif
