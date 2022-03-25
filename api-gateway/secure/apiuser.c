// Copyright (C) 2020 Intel Corporation

/* compile: (require libdb5.3-dev)
   gcc -o apiuser apiuser.c -ldb -lcrypt
*/

#include <sys/random.h>
#include <crypt.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>

#define DB_DBM_HSEARCH 1
#include <db.h>

#define SALT_LEN 8

char* salt(char *buf, size_t len) {
    const char chars[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWAXY"
                         "abcdefghijklmnopqrstuvwxyz";
    getrandom(buf, --len, 0);
    for (char* p = buf + len; p >= buf; --p)
        *p = chars[*p % (sizeof(chars) - 1)];
    buf[len] = '\0';
    return buf;
}

int main(int argc, char* argv[]) {
    const char *file = "apiuser";
    enum { DELETE = -1, LIST, CHANGE, NEW, UPDATE } action = LIST;
    int verbose = 0;
    int strict = 0;
    char *p;

    /* parse arguments */
    int opt;
    while ((opt = getopt(argc, argv, "cdf:hlnqsuv")) != -1) {
        switch (opt) {
            case 'l':  /* list */
                action = LIST;
                break;
            case 'c':  /* change */
                action = CHANGE;
                break;
            case 'n':  /* new */
                action = NEW;
                break;
            case 'u':  /* update */
                action = UPDATE;
                break;
            case 'd':  /* delete */
                action = DELETE;
                break;
            case 'f':  /* db file */
                file = optarg;
                if ((p = strrchr(file, '.')) && !strcmp(p, ".db"))
                    *p = '\0';
                break;
            case 's':  /* strict */
                strict = 1;
                break;
            case 'q':  /* quiet */
                verbose = -1;
                break;
            case 'v':  /* verbose */
                verbose = 1;
                break;
            case 'h':
                printf(" %s [-v|-q] [-s] [-f db_file] ...\n"
                       "	list:    [-l] [username ...]\n"
                       "	new:     -n username[:password] ...\n"
                       "	change:  -c username[:password] ...\n"
                       "	update:  -u username[:password] ...\n"
                       "	delete:  -d username ...\n", argv[0]);
                return 0;
        }
    }

    int flags = (action == LIST ? O_RDONLY : O_RDWR) |
                (action >= NEW ? O_CREAT : 0);
    DBM *db = dbm_open(file, flags, S_IRUSR | S_IWUSR);
    if (!db) {
        if (verbose >= 0)
            fprintf(stderr, "open database (%s.db) failed\n", file);
        return 2;
    }

    datum user = { NULL, 0 };
    datum pass = { NULL, 0 };
    int rc = 0;
    if (optind >= argc) {
        /* list all users */
        for (user = dbm_firstkey(db); user.dptr; user = dbm_nextkey(db)) {
            printf("%.*s", user.dsize, user.dptr);
            if (verbose > 0) {
                pass = dbm_fetch(db, user);
                printf(":%.*s", pass.dsize, pass.dptr);
            }
            puts("");
        }
    } else {
        for (; optind < argc; ++optind) {
            user.dptr = argv[optind];
            user.dsize = strlen(user.dptr);
            if (action == LIST) {  /* list specified users */
                pass = dbm_fetch(db, user);
                rc = pass.dptr ? 0 : -1;
                if (!rc && verbose >= 0) {
                    printf("%.*s", user.dsize, user.dptr);
                    if (verbose > 0)
                        printf(":%.*s", pass.dsize, pass.dptr);
                    puts("");
                }
            } else if (action == DELETE) {  /* delete users */
                rc = dbm_delete(db, user) ? -1 : 0;
            } else {  /* change/new/update users */
                p = strchr(argv[optind], ':');
                if (p) {  /* password in argument */
                    user.dsize = p - user.dptr;
                    *(p++) = '\0';
                }

                if (action != UPDATE) {  /* check existence */
                    if (dbm_fetch(db, user).dptr)
                        rc = action == NEW;
                    else if (action == CHANGE)
                        rc = -1;
                    if (rc)
                        goto error;
                }

                if (!p) {  /* read password from tty */
                    char prompt[16 + user.dsize];
                    sprintf(prompt, "password of %s: ", user.dptr);
                    p = getpass(prompt);
                }

                /* hash password with random salt */
                char buf[3 + SALT_LEN + 1] = "$6$";
                salt(buf + 3, SALT_LEN + 1);
                pass.dptr = crypt(p, buf);
                pass.dsize = strlen(pass.dptr);

                rc = dbm_store(db, user, pass, DBM_REPLACE) ? 1 : 0;
            }

            if (rc) {
error:
                if (verbose >= 0)
                    fprintf(stderr, "user '%s' %s\n", user.dptr,
                            rc < 0 ? "not found" : "already exist");
                if (strict)
                    break;
            }
        }
    }

    dbm_close(db);
    return rc ? rc + 2 : rc;
}
