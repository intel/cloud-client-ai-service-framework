#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // for vasprintf
#endif

#if HAVE_NTP_GETTIME
#include <sys/timex.h>
#endif

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"

#define HOSTNAME_MAX 255
#define APPNAME_MAX  48
#define PRIORITY_MAX 5
#define TIME3164_MAX 15
#define TIME5424_MAX 32
#define PROCID_MAX   12
#define MSGID_MAX    32
#define HDR3164_MAX  (PRIORITY_MAX + TIME3164_MAX + 1 + HOSTNAME_MAX + 1 + APPNAME_MAX + PROCID_MAX + 2)
#define HDR5424_MAX  (PRIORITY_MAX + 2 + TIME5424_MAX + 1 + HOSTNAME_MAX + 1 + APPNAME_MAX + PROCID_MAX + 1 + MSGID_MAX + 1)
#define HEADER_MAX   HDR5424_MAX

#define NILVALUE "-"
#define DEFAULT_SERVER "/dev/log"

enum {
    TYPE_UDP = 1 << 1,
    TYPE_TCP = 1 << 2,
    TYPE_ANY = TYPE_UDP | TYPE_TCP
};

typedef struct log_t {
    int   fd;                            /* socket of connection */
    int   output;                        /* copy message to this fd */
    pid_t pid;
    int   facility;
    char  app[APPNAME_MAX + 1];          /* app-name */
    char  msgid[MSGID_MAX + 1];          /* msgid (see RFC5424 #6.2.7) */
    char *hostname;                      /* hostname appeared in message */
    char *server;                        /* server name (with port) */
    union {
        struct {
            unsigned local        :1;    /* AF_UNIX socket */
            unsigned stream       :1;    /* SOCK_STREAM type socket */
            unsigned rfc5424      :1;    /* RFC5424 compliance */
            unsigned rfc5424_time :1;    /* include time stamp */
            unsigned rfc5424_tq   :1;    /* include time quality markup */
            unsigned rfc5424_host :1;    /* include hostname */
            unsigned octet_count  :1;    /* use RFC6587 octet counting */
        };
        unsigned flags;
    };
} log_t;

size_t log_size(void) {
    return sizeof(log_t);
}

#if !defined(__APPLE__) && !defined(__sun) && \
    !defined(__FreeBSD__) && !defined(__DragonFly__) && \
    !defined(__OpenBSD__) && !defined(__NetBSD__)
#if defined(_GNU_SOURCE)
#include <errno.h>
#else
extern char *__progname;
#endif
static inline const char* getprogname(void) {
#if defined(_GNU_SOURCE)
    return program_invocation_short_name;
#else
    return __progname;
#endif
}
#endif


static int unix_socket(const char *path, int type) {
    int fd = -1;
    struct sockaddr_un addr;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    addr.sun_family = AF_UNIX;
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));
    if (addr.sun_path[sizeof(addr.sun_path) - 1]) {
        errno = ENAMETOOLONG;
        return -1;  /* pathname too long */
    }
#pragma GCC diagnostic pop

    for (int i = 2; i; --i) {
        int st = -1;

        if (i == 2 && type & TYPE_UDP) {
            st = SOCK_DGRAM;
        } else if (i == 1 && type & TYPE_TCP) {
            st = SOCK_STREAM;
        } else {
            continue;
        }
        if ((fd = socket(AF_UNIX, st, 0)) >= 0) {
            if (!connect(fd, (struct sockaddr *)&addr, sizeof(addr)))
                break;
            close(fd);
            fd = -1;
        }
    }
    return fd;
}

static int inet_socket(const char *server, int type) {
    int fd = -1;

    /* get host and port */
    const char *port = strrchr(server, ':');
    char host[strlen(server)];
    if (port) {
        strncpy(host, server, port - server);
        host[port++ - server] = '\0';
        server = host;
    } else {
        port = "syslog";
    }

    for (int i = 2; i; --i) {
        int ec;
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        if (i == 2 && type & TYPE_UDP) {
            hints.ai_socktype = SOCK_DGRAM;
        } else if (i == 1 && type & TYPE_TCP) {
            hints.ai_socktype = SOCK_STREAM;
        } else {
            continue;
        }
        if (!(ec = getaddrinfo(server, port, &hints, &res))) {
            if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) >= 0) {
                if (!connect(fd, res->ai_addr, res->ai_addrlen)) {
                    i = 1;
                } else {
                    close(fd);
                    fd = -1;
                }
            }
            freeaddrinfo(res);
        } else {
            fprintf(stderr, "failed to resolve %s:%s: %s\n",
                    server, port, gai_strerror(ec));
        }
    }
    return fd;
}

static int log_connect0(log_t* log, const char* server, int type) {
    if (server[0] == '/') { /* unix domain */
        log->fd = unix_socket(server, type);
        log->local = 1;
    } else { /* network */
        log->fd = inet_socket(server, type);
        log->local = 0;
    }
    if (log->fd < 0) return 0;

    socklen_t len = sizeof(type);
    getsockopt(log->fd, SOL_SOCKET, SO_TYPE, &type, &len);
    log->stream = type == SOCK_STREAM;
    return 1;
}

static int log_reconnect(log_t* log) {
    int type = TYPE_ANY;
    if (log->fd >= 0) {
        /* retrieve old socket type */
        socklen_t len = sizeof(type);
        getsockopt(log->fd, SOL_SOCKET, SO_TYPE, &type, &len);
        switch (type) {
            case SOCK_STREAM: type = TYPE_TCP; break;
            case SOCK_DGRAM:  type = TYPE_UDP; break;
            default: assert(0);
        }
        close(log->fd);
    }
    return log_connect0(log, log->server, type);
}

static int log_connect(log_t* log, const char* server) {
    int type = TYPE_ANY;
    if (!strncmp(server, "unix://", 7)) {
        server += 7;
        if (server[0] != '/')
            --server;
    } else if (!strncmp(server, "udp://", 6)) {
        type = TYPE_UDP;
        server += 6;
    } else if (!strncmp(server, "tcp://", 6)) {
        type = TYPE_TCP;
        server += 6;
    }
    log->server = strdup(server);
    return log_connect0(log, server, type);
}

int log_setapp(log_t* log, pid_t pid, const char* app) {
    char path[PATH_MAX];

    if (!pid || pid == getpid()) {
        pid = getpid();
        if (!app)
            app = getprogname();
    } else if (!geteuid() && !kill(pid, 0)) {
        /* only root can send log as any process */
        if (!app) {
            sprintf(path, "/proc/%u/exe", pid);
            (void)!readlink(path, &path[0], sizeof(path));
            app = strrchr(path, '/');
            if (app)
                ++app;
            else
                app = path;
        }
    } else {
        return 0;
    }

    strncpy(log->app, app, APPNAME_MAX);
    log->app[APPNAME_MAX] = '\0';
    log->pid = pid;
    return 1;
}

void log_sethostname(log_t* log, const char* host) {
    if (log->local) return;

    char buf[HOSTNAME_MAX + 1];
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    int is_ip = 0;

    if (host) {
        if (log->rfc5424)
            log->rfc5424_host = !!host[0];
        is_ip = inet_ntop(AF_INET, host, (char*)&addr, len) ||
                inet_ntop(AF_INET6, host, (char*)&addr, len);
        strncpy(buf, host, sizeof(buf));
        buf[HOSTNAME_MAX] = '\0';
    } else if (!gethostname(buf, sizeof(buf))) {
        /* system hostname */
    } else if (!getsockname(log->fd, (struct sockaddr*)&addr, &len)) {
        /* ipv4 or ipv6 address */
        char *ip = addr.ss_family == AF_INET ?
            (char*)&((struct sockaddr_in*)&addr)->sin_addr :
            (char*)&((struct sockaddr_in6*)&addr)->sin6_addr;
        inet_ntop(addr.ss_family, ip, buf, sizeof(buf));
        is_ip = 1;
    } else {
        strcpy(buf, "unknown");
    }

    if (!is_ip && !log->rfc5424) {
        /* trim domain name (see RFC3164 #5.2) */
        char *p = strchr(buf, '.');
        if (p) *p = '\0';
    }

    free(log->hostname);
    log->hostname = buf[0] ? strdup(buf) : NULL;
}

void log_setoutput(log_t* log, int fd) {
    log->output = fd;
}

void log_setfacility(log_t* log, int fac) {
    log->facility = fac & LOG_FACMASK;
}

void log_rfc5424opts(log_t* log, int time, int tq, int host, const char* msgid) {
    log->rfc5424_time = time;
    log->rfc5424_tq = tq;

    if (!(log->rfc5424_host = host) && log->hostname) {
        free(log->hostname);
        log->hostname = NULL;
    }

    if (msgid && msgid[0]) {
        strncpy(log->msgid, msgid, MSGID_MAX);
        log->msgid[MSGID_MAX] = '\0';
    } else {
        strcpy(log->msgid, NILVALUE);
    }

    log->rfc5424 = time | host | tq | !!msgid;
}


log_t* log_init(void* buf, const char* server) {
    log_t* log = (log_t*)buf;

    memset(log, 0, sizeof(log_t));
    log->output = -1;
    log->facility = LOG_USER;
    strcpy(log->msgid, NILVALUE);

    if (log_connect(log, server ?: DEFAULT_SERVER)) {
        log_setapp(log, 0, NULL);
        if (!log->local)
            log_sethostname(log, NULL);
    }
    return log->fd >= 0 ? log : NULL;
}

void log_fini(log_t* log) {
    if (!log)
        return;

    if (log->fd >= 0) {
        close(log->fd);
        log->fd = -1;
    }
    free(log->server);
    free(log->hostname);
    log->server = log->hostname = NULL;
}

void log_send(log_t* log, int pri, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vsend(log, pri, fmt, ap);
    va_end(ap);
}

void log_vsend(log_t* log, int pri, const char* fmt, va_list ap) {
    struct iovec iov[5];
    if (!LOG_FAC(pri))
        pri = LOG_MAKEPRI(log->facility, LOG_PRI(pri));
    else
        pri &= ~(LOG_FACMASK | LOG_PRIMASK);

    /* 0: octet-count (see RFC6587 #3.4.1) */
    char cnt[8];
    iov[0].iov_base = cnt;
    iov[0].iov_len = 0;

    /* 1: message header */
    char hdr[HEADER_MAX + 1];
    int l1 = snprintf(hdr, 6, "<%u>", pri);
    struct timeval tv;
    struct tm tm;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    if (!log->rfc5424) {
        l1 += strftime(hdr + l1, TIME3164_MAX + 2, "%b %d %H:%M:%S ", &tm);
    } else if (log->rfc5424_time) {  /* see RFC5424 #6.2.3 */
        char f[TIME5424_MAX + 2];
        int i = strftime(f, sizeof(f), "1 %Y-%m-%dT%H:%M:%S.%%06u%z  ", &tm);
        f[i - 2] = f[i - 3];
        f[i - 3] = f[i - 4];
        f[i - 4] = ':';
        l1 += snprintf(hdr + l1, TIME5424_MAX + 4, f, tv.tv_usec);
    } else {
        l1 += sprintf(hdr + l1, "1 " NILVALUE " ");
    }
    if (!log->local && log->rfc5424) {
        const char *host = log->rfc5424_host ? log->hostname : NILVALUE;
        l1 += snprintf(hdr + l1, sizeof(hdr) - l1, "%s %s %u %s ", host, log->app, (unsigned)log->pid, log->msgid);
    } else if (log->hostname) {
        l1 += snprintf(hdr + l1, sizeof(hdr) - l1, "%s %s[%u]: ", log->hostname, log->app, (unsigned)log->pid);
    } else {
        l1 += snprintf(hdr + l1, sizeof(hdr) - l1, "%s[%u]: ", log->app, (unsigned)log->pid);
    }
    iov[1].iov_base = hdr;
    iov[1].iov_len = l1;

    /* 2: structured data */
    iov[2].iov_base = NULL;
    iov[2].iov_len = 0;
    if (!log->local && log->rfc5424) {
        if (log->rfc5424_time && log->rfc5424_tq) {
            /* timeQulity (see RFC5424 #7.1) */
            char acc[15 + 20 + 2] = { '\0' };
#ifdef HAVE_NTP_GETTIME
            struct ntptimeval ntptv;
            if (ntp_gettime(&ntptv) == TIME_OK)
                snprintf(acc, sizeof(acc), " syncAccuracy=\"%ld\"", ntptv.maxerror);
#endif
            iov[2].iov_len = asprintf((char**)&iov[2].iov_base, "[timeQuality tzKnown=\"1\" isSynced=\"%1d\"%s] ", !!*acc, acc);
        } else
            iov[2].iov_len = asprintf((char**)&iov[2].iov_base, NILVALUE " ");
        /* TODO: add support of user defined structured data */
    }

    /* 3: message body */
    char *msg;
    int l2 = vasprintf(&msg, fmt, ap);
    while (l2 && isspace(msg[l2 - 1]))
        msg[--l2] = '\0';  /* trim trailing space */
    if (!log->local && !log->rfc5424 && l1 + l2 > 1024)
        l2 = 1024 - l1;    /* see RFC3164 #4.1 */
    iov[3].iov_base = msg;
    iov[3].iov_len = l2;

    /* 4: frame trailer (see RFC6545 #3.4.2) */
    iov[4].iov_base = "\n";
    iov[4].iov_len = 1;

    struct msghdr message = { 0 };
    message.msg_iov = iov + 1;
    message.msg_iovlen = 3;
    if (log->octet_count) {
        iov[0].iov_len = snprintf(cnt, sizeof(cnt), "%u",
                                  (unsigned)(iov[1].iov_len + iov[2].iov_len + iov[3].iov_len));
        --message.msg_iov;
    } else if (log->stream) {
        ++message.msg_iovlen;
    }

#ifdef SCM_CREDENTIALS
    /* server may follow local socket credentials rather in message */
    struct cmsghdr *cmhp;
    struct ucred *cred;
    union {
        struct cmsghdr cmh;
        char   control[CMSG_SPACE(sizeof(struct ucred))];
    } cbuf;
    if (log->pid != getpid() && !kill(log->pid, 0)) {
        message.msg_control = cbuf.control;
        message.msg_controllen = CMSG_SPACE(sizeof(struct ucred));
        cmhp = CMSG_FIRSTHDR(&message);
        cmhp->cmsg_len = CMSG_LEN(sizeof(struct ucred));
        cmhp->cmsg_level = SOL_SOCKET;
        cmhp->cmsg_type = SCM_CREDENTIALS;
        cred = (struct ucred*) CMSG_DATA(cmhp);
        cred->pid = log->pid;
    }
#endif

    if (sendmsg(log->fd, &message, MSG_NOSIGNAL) < 0) {
        /* disconnected? retry once */
        log_reconnect(log);
        if (sendmsg(log->fd, &message, MSG_NOSIGNAL) < 0)
            perror("send message failed");
    }
    if (log->output >= 0) {
        /* copy message without structured data */
        iov[2].iov_len = 0;
        (void)!writev(log->output, iov + 1, 4);
    }

    free(iov[2].iov_base);
    free(msg);
}

#ifdef TEST
int main(int argc, char* argv[]) {
    const char *server =  argc > 2 ? argv[1] : NULL;
    const char *msg = argc > 2 ? argv[2] : argv[1];

    log_t log;
    if (msg && log_init(&log, server)) {
        log_setoutput(&log, STDERR_FILENO);
        log_send(&log, LOG_NOTICE, argc > 2 ? argv[2] : argv[1]);
        log_fini(&log);
    }
    return 0;
}
#endif
