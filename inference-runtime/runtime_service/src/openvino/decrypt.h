// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <openssl/engine.h>
#include <openssl/pem.h>

#ifdef __cplusplus
extern "C" {
#endif

ENGINE* load_crypto_engine(const char* name);
EVP_PKEY* load_private_key(ENGINE *eng, const char *path);
void* decrypt(ENGINE* eng, EVP_PKEY* pkey, int fd, size_t* size);

#ifdef __cplusplus
}
#endif
