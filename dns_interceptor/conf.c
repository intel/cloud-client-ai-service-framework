#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "conf.h"

struct host_ent {
    int family;
    union {
        struct in_addr addr;
        struct in6_addr addr6;
    };
    char names[0];
};

struct host_ent** hosts = NULL;

static int parse_line(size_t lineno, char* line, size_t len, int state) {
    static const int step = 8;
    static int count = 0;

    char *p, *q;
    for (p = line; isspace(*p); ++p);
    if (*p == '\0' || *p == '#') return -1;

    for (q = p + 1; *q && !isspace(*q); ++q);
    if (*q) {
        *q++ = '\0';
        for (; isspace(*q); ++q);
    }
    if (!*q) return -1;  // missing domain name

    if (count % step == 0) {
        // enlarge array size
        hosts = realloc(hosts, (count + step + 1) * sizeof(void*));
    }

    // parse IP address
    struct host_ent ent;
    if (inet_pton(AF_INET, p, &ent.addr)) {
        ent.family = AF_INET;
    } else if (inet_pton(AF_INET6, p, &ent.addr6)) {
        ent.family = AF_INET6;
    } else {
        return -1;  // malformatted address
    }

    // append entry
    hosts[count] = malloc(sizeof(struct host_ent) + len + 1);
    memcpy(hosts[count], &ent, sizeof(ent));
    memcpy(hosts[count]->names, q, len + 1);
    hosts[++count] = NULL;
    return 0;
}

void conf_read(int fd, char* buf, size_t size) {
    size_t lineno = 0;
    int state = 0;
    size_t len, cur = 0;
    while ((len = read(fd, buf + cur, size - cur))) {
        // process lines in buffer
        char *p, *q;
        for (p = buf; (q = strchr(p, '\n')); p = q + 1) {
            *q = '\0';
            state = parse_line(++lineno, p, q - p, state);
        }
        if (p > buf) {
            // partial line left in buffer
            cur = len + cur - (p - buf);
            memmove(buf, p, cur);
        } else {
            // the line is too long, skip it
            while (len = read(fd, buf, size)) {
                if ((p = strchr(buf, '\n'))) {
                    cur = len - (++p - buf);
                    memmove(buf, p, cur);
                    break;
                }
            }
            ++lineno;
        }
    }
    if (cur) {
        // the last line without endding '\n'
        buf[cur] = '\0';
        state = parse_line(lineno + 1, buf, cur, state);
    }
}

int conf_init(const char* path) {
    char buf[256];
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        conf_read(fd, buf, sizeof(buf));
        close(fd);
    }
    if (!hosts)
        hosts = calloc(1, sizeof(void*));
    return fd >= 0;
}

void conf_fini(void) {
    if (hosts) {
        for (int i = 0; hosts[i]; ++i) {
            free(hosts[i]);
        }
        free(hosts);
        hosts = NULL;
    }
}

void* conf_getaddr(int family, const char* name, void *addr) {
    size_t len = strlen(name);
    size_t size = family == AF_INET ?
                  sizeof(struct in_addr) : sizeof(struct in6_addr);
    struct host_ent *ent = NULL;
    for (int i = 0; hosts[i]; ++i) {
        char *p;
        if ((p = strstr(hosts[i]->names, name)) &&
            (p == hosts[i]->names || isspace(p[-1])) &&
            (p[len] == '\0' || isspace(p[len]))) {
            if (hosts[i]->family == family)
                return memcpy(addr, &hosts[i]->addr, size);
            else if (family == AF_INET6)
                ent = hosts[i]; // ipv4 candidate address
        }
    }
    if (!ent) return NULL;

    // map ipv4 address to ipv6
    if (ent->addr.s_addr == htonl(INADDR_LOOPBACK)) {
        memcpy(addr, &in6addr_loopback, sizeof(in6addr_loopback));
    } else {
        uint8_t *p = ((struct in6_addr*)addr)->__in6_u.__u6_addr8;
        memset(p, 0, 10);
        memset(p, 0xff, 2);
        memcpy(p + 12, &ent->addr, sizeof(struct in_addr));
    }
    return addr;
}

#ifdef TEST
#include <stdio.h>
int main(int argc, char* argv[]) {
    if (argc < 2 || !conf_init(argv[1]))
        return 1;

    if (argc > 2) {
        // find entry
        char buf[256];
        struct in6_addr addr;
        if ((conf_getaddr(AF_INET, argv[2], &addr) &&
            inet_ntop(AF_INET, &addr, buf, sizeof(buf))) ||
            (conf_getaddr(AF_INET6, argv[2], &addr) &&
            inet_ntop(AF_INET6, &addr, buf, sizeof(buf))))
            printf("%s\n", buf);
    } else {
        // dump entries
        for (int i = 0; hosts[i]; ++i) {
            printf("%s\n", hosts[i]->names);
        }
    }
    conf_fini();
    return 0;
}
#endif
