#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <nss.h>
#include <string.h>

#include "conf.h"


#if 0  // no need to support this inferface
enum nss_status
_nss_ccai_gethostbyname4_r(const char *name, struct gaih_addrtuple **pat,
                           char *buffer, size_t buflen, int *errnop,
                           int *herrnop, int32_t *ttlp) {
    return NSS_STATUS_UNAVAIL;
}
#endif

#if 0  // not support cannonical name
enum nss_status
_nss_ccai_getcanonname_r(const char *name, char *buffer, size_t buflen,
                         char **result,int *errnop, int *h_errnop) {
    return NSS_STATUS_UNAVAIL;
}
enum nss_status
_nss_ccai_gethostbyname3_r(const char *name, int af, struct hostent *result,
                           char *buffer, size_t buflen, int *errnop,
                           int *herrnop, int32_t *ttlp, char **canonp) {
    return NSS_STATUS_SUCCESS;
}
enum nss_status
_nss_ccai_gethostbyname2_r(const char *name, int af, struct hostent *result,
                           char *buffer, size_t buflen, int *errnop,
                           int *herrnop) {
    return _nss_ccai_gethostbyname3_r(name, af, result, buffer, buflen,
                                      errnop, herrnop, NULL, NULL);
}
#else

enum nss_status
_nss_ccai_gethostbyname2_r(const char *name, int af, struct hostent *result,
                           char *buffer, size_t buflen, int *errnop,
                           int *herrnop) {
    struct {  // working buffer
        char* addrp;
        char* nil;
        union {
            struct in_addr addr;
            struct in6_addr addr6;
        };
        char name[0];
    } *buf = (void*)buffer;

    // check address family
    size_t addrlen;
    switch (af) {
        case AF_INET:
            addrlen = sizeof(struct in_addr);
            break;
        case AF_INET6:
            addrlen = sizeof(struct in6_addr);
            break;
        default:
            *herrnop = NO_DATA;
            *errnop = EAFNOSUPPORT;
            return NSS_STATUS_UNAVAIL;
    }

    // check buffer size
    size_t namelen = strlen(name);
    if (sizeof(*buf) + namelen >= buflen) {
        *errnop = ERANGE;
        *herrnop = NETDB_INTERNAL;
        return NSS_STATUS_TRYAGAIN;
    }

    buf->addrp = (char*)&buf->addr;
    buf->nil = NULL;
    if (!conf_getaddr(af, name, buf->addrp)) {
        *herrnop = HOST_NOT_FOUND;
        return NSS_STATUS_NOTFOUND;
    }
    memcpy(buf->name, name, namelen + 1);

    result->h_name = buf->name;
    result->h_aliases = &buf->nil;
    result->h_addrtype = af;
    result->h_length = addrlen;
    result->h_addr_list = &buf->addrp;

    *herrnop = NETDB_SUCCESS;
    return NSS_STATUS_SUCCESS;
}

#endif

enum nss_status
_nss_ccai_gethostbyname_r(const char *name, struct hostent *result,
                          char *buffer, size_t buflen, int *errnop,
                          int *herrnop) {
    return _nss_ccai_gethostbyname2_r(name, AF_INET, result, buffer, buflen,
                                      errnop, herrnop);
}


#if 0  // TODO: reverse resolving
enum nss_status
_nss_ccai_gethostbyaddr2_r(const void *addr, socklen_t len, int af,
                           struct hostent *result, char *buffer, size_t buflen,
                           int *errnop, int *h_errnop, int32_t *ttlp) {
    return NSS_STATUS_UNAVAIL;
}
enum nss_status
_nss_ccai_gethostbyaddr_r(const void *addr, socklen_t len, int af,
                          struct hostent *result, char *buffer, size_t buflen,
                          int *errnop, int *h_errnop) {
    return _nss_ccai_gethostbyaddr2_r(addr, len, af, result, buffer, buflen,
                                      errnop, h_errnop, NULL);
}
#endif
