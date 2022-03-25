// Copyright (C) 2020 Intel Corporation
//

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/ui.h>

void* decrypt(ENGINE* eng, EVP_PKEY* pkey, int fd, size_t* size) {
    const EVP_CIPHER *cipher = EVP_aes_256_cbc();
    size_t key_len = EVP_PKEY_size(pkey);
    unsigned char buf[128 * 1024];

    /* read header ( iv and encrypted key) */
    int len = EVP_MAX_IV_LENGTH + key_len;
    if (read(fd, buf, len) != len)
        return NULL;

    /*** decrypt aes key ***/
    int rv = 0;
    void *key = buf + EVP_MAX_IV_LENGTH;
    void *out = key + key_len;
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, eng);
    if (pctx) {
        if (EVP_PKEY_decrypt_init(pctx) > 0)
            rv = EVP_PKEY_decrypt(pctx, out, &key_len, key, key_len);
        EVP_PKEY_CTX_free(pctx);
    }
    if (rv <= 0 || key_len != EVP_CIPHER_key_length(cipher))
        return NULL;

    /*** decrypt data ***/
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return NULL;

    /* init */
    if ((rv = EVP_DecryptInit_ex(ctx, cipher, eng, NULL, NULL)) > 0 &&
        (rv = EVP_CIPHER_CTX_set_key_length(ctx, key_len)) > 0 &&
        (rv = EVP_DecryptInit_ex(ctx,  NULL, NULL, out, buf)) > 0) {
        /* allocate plaintext buffer */
        struct stat *st = (struct stat*)buf;
        fstat(fd, st);
        len = st->st_size - len;
        if (len < EVP_MAX_BLOCK_LENGTH) {
            EVP_CIPHER_CTX_free(ctx);
            return NULL;
        }
        out = malloc(len + EVP_MAX_BLOCK_LENGTH);
    }

    /* update */
    *size = 0;
    for (void *p = out; rv > 0; p += len) {
        len = read(fd, buf, sizeof(buf));
        if (!len) break;
        rv = EVP_DecryptUpdate(ctx, p, &len, buf, len);
        *size += len;
    }

    /* finalize */
    if (rv <= 0 || !EVP_DecryptFinal_ex(ctx, out + *size, &len)) {
        free(out);
        out = NULL;
    }
    EVP_CIPHER_CTX_free(ctx);

    *size += len;
    return out;
}

ENGINE* load_crypto_engine(const char* name) {
    ENGINE *eng;

    if (!name || !*name)
        return NULL;

    ENGINE_load_builtin_engines();
    if ((eng = ENGINE_by_id(name)) == NULL &&
        (eng = ENGINE_by_id("dynamic")) != NULL) {
        const char* cmd = access(name, F_OK) ? "ID" : "SO_PATH";
        if (!ENGINE_ctrl_cmd_string(eng, cmd, name, 0) ||
            !ENGINE_ctrl_cmd_string(eng, "LOAD", NULL, 0)) {
            ENGINE_free(eng);
            return NULL;  /* load failed */
        }
    }

    if (!eng ||
        (!ENGINE_init(eng) && ENGINE_free(eng)) ||
        (!ENGINE_set_default(eng, ~0U) && ENGINE_finish(eng)))
        return NULL;  /* init failed */
    return eng;
}

EVP_PKEY* load_private_key(ENGINE *eng, const char *path) {
    EVP_PKEY *pkey = NULL;
    int fd;
    if (eng) {
        pkey = ENGINE_load_private_key(eng, path, (UI_METHOD*)UI_null(), NULL);
    } else if ((fd = open(path, O_RDONLY)) >= 0) {
        BIO *bio = BIO_new_fd(fd, BIO_CLOSE);
        PEM_read_bio_PrivateKey(bio, &pkey, NULL, NULL);
        BIO_free(bio);
    }
    return pkey;
}

#ifdef TEST
#include <limits.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    const char* key_file = NULL;
    const char* dat_file = NULL;
    const char* eng_name = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "e:f:hk:")) != -1) {
        switch (opt) {
            case 'e':
                eng_name = optarg;
                break;
            case 'f':
                if (strcmp(optarg, "-"))
                    dat_file = optarg;
                break;
            case 'k':
                key_file = optarg;
                break;
            case 'h':
                fprintf(stderr, "%s [-e engine] [-k key] [-f] [input]\n", argv[0]);
            default:
                exit(1);
        }
    }

    if (!dat_file) {
        if (argc > optind)
            dat_file = argv[optind++];
        else
            dat_file = "/dev/stdin";
    }

    ENGINE* eng = NULL;
    if (eng_name) {
        eng = load_crypto_engine(eng_name);
        if (!eng) {
            fprintf(stderr, "load engine failed\n");
            return 1;
        }
        fprintf(stderr, "Engine %s(%s) is loaded\n",
                ENGINE_get_id(eng), ENGINE_get_name(eng));
    }

    char path[PATH_MAX];
    if (!key_file && !eng) {
        const char* home = getenv("HOME");
        if (!home) home = "";
        sprintf(path, "%s/.ssh/id_rsa", home);
        key_file = path;
    }
    if (access(key_file, R_OK)) {
        fprintf(stderr, "key not found\n");
        return 1;
    }

    /* read private key */
    EVP_PKEY *pkey = load_private_key(eng, key_file);
    if (!pkey) {
        fprintf(stderr, "load key failed\n");
        return 1;
    }

    void *buf = NULL;
    size_t size = 0;
    FILE *fp = fopen(dat_file, "rb");
    if (fp) {
        if (!(buf = decrypt(NULL, pkey, fileno(fp), &size)))
            fprintf(stderr, "decrypt failed: %s\n",
                    ERR_error_string(ERR_get_error(), NULL));
        fclose(fp);
    } else {
        fprintf(stderr, "input file open failed\n");
    }
    EVP_PKEY_free(pkey);
    ENGINE_finish(eng);
    if (!buf) return 1;

    /* output plaintext */
    fp = fopen(argc > optind ? argv[optind] : "/dev/stdout", "wb");
    fwrite(buf, size, 1, fp);
    fclose(fp);
    free(buf);
    return 0;
}
#endif
