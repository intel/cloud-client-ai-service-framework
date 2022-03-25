#include <fcntl.h>
#include <unistd.h>

#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/ui.h>


int encrypt(ENGINE* eng, EVP_PKEY* pkey, int fd, int ofd) {
    unsigned char ibuf[64 * 1024];
    unsigned char obuf[64 * 1024];
    unsigned char *iv = obuf;
    unsigned char *key = iv + EVP_MAX_IV_LENGTH;
    size_t key_len = EVP_PKEY_size(pkey);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -3;

    /* init */
    int rv = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), eng, NULL, NULL) > 0 &&
        RAND_bytes(iv, EVP_CIPHER_CTX_iv_length(ctx)) > 0 &&
        EVP_CIPHER_CTX_rand_key(ctx, ibuf) > 0 &&
        EVP_EncryptInit_ex(ctx, NULL, NULL, ibuf, iv) > 0) {
        /* encrypt key */
        EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new(pkey, eng);
        if (pctx) {
            if (EVP_PKEY_encrypt_init(pctx) > 0) {
                rv = EVP_PKEY_encrypt(pctx, key, &key_len, ibuf,
                                      EVP_CIPHER_CTX_key_length(ctx));
                /* write header */
                !write(ofd, iv, EVP_MAX_IV_LENGTH + EVP_PKEY_size(pkey));
            }
            EVP_PKEY_CTX_free(pctx);
        }
    }

    /* update */
    int len;
    while (rv > 0 &&
           (len = read(fd, ibuf, sizeof(ibuf) - EVP_MAX_BLOCK_LENGTH))) {
        rv = EVP_EncryptUpdate(ctx, obuf, &len, ibuf, len);
        !write(ofd, obuf, len);
    }

    /* finalize */
    if (rv > 0 && (rv = EVP_EncryptFinal_ex(ctx, obuf, &len)))
        !write(ofd, obuf, len);

    EVP_CIPHER_CTX_free(ctx);
    if (rv <= 0) {
        !ftruncate(ofd, 0);
        return -1;
    }
    return 0;
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

EVP_PKEY* load_public_key(ENGINE *eng, const char *path) {
    EVP_PKEY *pkey = NULL;
    int fd;
    if (eng) {
        /* tpm2tss engine doesn't support public key methods */
        pkey = ENGINE_load_private_key(eng, path, (UI_METHOD*)UI_null(), NULL);
    } else if ((fd = open(path, O_RDONLY)) >= 0) {
        BIO *bio = BIO_new_fd(fd, BIO_CLOSE);
        PEM_read_bio_PUBKEY(bio, &pkey, NULL, NULL);
        BIO_free(bio);
    }
    return pkey;
}


#include <limits.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    const char* key_file = NULL;
    const char* in_file  = NULL;
    const char* out_file = NULL;
    const char* eng_name = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "e:f:hk:o:")) != -1) {
        switch (opt) {
            case 'e':
                eng_name = optarg;
                break;
            case 'f':
                if (strcmp(optarg, "-"))
                    in_file = optarg;
                break;
            case 'k':
                key_file = optarg;
                break;
            case 'o':
                if (strcmp(optarg, "-"))
                    out_file = optarg;
                break;
            case 'h':
                fprintf(stderr, "%s [-e engine] [-k key] [-f] [input]\n", argv[0]);
            default:
                exit(1);
        }
    }

    if (!in_file) {
        if (argc > optind)
            in_file = argv[optind++];
        else
            in_file = "/dev/stdin";
    }
    if (!out_file) {
        if (argc > optind)
            out_file = argv[optind++];
        else {
            out_file = "/dev/stdout";
        }
    }

    /* load engine */
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

    /* read public key */
    EVP_PKEY *pkey = NULL;
    if (key_file) {
        pkey = load_public_key(eng, key_file);
    } else if (!eng) {
        /* load default openssh public key */
        const char* cmd = "exec ssh-keygen -e -m PKCS8 -f ~/.ssh/id_rsa.pub";
        FILE *fp = popen(cmd, "r");
        if (fp) {
            PEM_read_PUBKEY(fp, &pkey, NULL, NULL);
            pclose(fp);
        }
    }
    if (!pkey) {
        fprintf(stderr, "load public key failed\n");
        ENGINE_finish(eng);
        return 2;
    }

    int rc = 0;
    int in_fd = open(in_file, O_RDONLY);
    int out_fd = open(out_file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (isatty(out_fd)) {
        close(out_fd);
        out_fd = open("/dev/null", O_WRONLY);
        fprintf(stderr, "discard output\n");
    }
    if (in_fd >= 0 && out_fd >= 0) {
        if (encrypt(NULL, pkey, in_fd, out_fd)) {
            fprintf(stderr, "encrypt failed: %s\n",
                    ERR_error_string(ERR_get_error(), NULL));
            rc = 4;
        }
        close(in_fd);
        close(out_fd);
    } else {
        fprintf(stderr, "open %sput failed\n", in_fd < 0 ? "in" : "out");
        rc = 3;
    }
    EVP_PKEY_free(pkey);
    ENGINE_finish(eng);
    return rc;
}
