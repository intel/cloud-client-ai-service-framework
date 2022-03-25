#pragma once
#ifndef NSS_CCAI_H
#define NSS_CCAI_H

#include <stddef.h>

int conf_init(const char* path);
void conf_fini(void);
void conf_read(int fd, char* buf, size_t size);
void* conf_getaddr(int family, const char* name, void *addr);

#endif
