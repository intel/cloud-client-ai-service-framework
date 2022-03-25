// Copyright (C) 2020 Intel Corporation


// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi
#include <syslog.h>
#include <string>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <fcgiapp.h>
#include <fcgio.h>
#include <fcgi_stdio.h>

#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0

int main(int argc, char **argv) {
    openlog("testfastcgi", LOG_CONS|LOG_NDELAY, LOG_USER);
    int err = FCGX_Init(); /* call before Accept in multithreaded apps */
    if (err) {
        syslog (LOG_INFO, "FCGX_Init failed: %d", err);
	return 1;
    }
    FCGX_Request cgi;
    err = FCGX_InitRequest(&cgi, LISTENSOCK_FILENO, LISTENSOCK_FLAGS);
    if (err) {
        syslog(LOG_INFO, "FCGX_InitRequest failed: %d", err);
	return 2;
    }
    while (1) {
        err = FCGX_Accept_r(&cgi);
        if (err) {
	    syslog(LOG_INFO, "FCGX_Accept_r stopped: %d", err);
	    break;
	}

        std::string result("Status: 200 OK\r\nContent-Type: text/plain\r\n"
                           "X-Content-Type-Options: nosniff\r\nX-frame-options: deny\r\n\r\n");

        const char* conf_dir="/etc/lighttpd/conf-enabled";
        DIR *dir = opendir(conf_dir);
        struct dirent *de;
        while (dir && (de = readdir(dir))) {
            FILE *fp;
            char buf[1024];
            snprintf(buf, sizeof(buf), "%s/%s", conf_dir, de->d_name);
            if (!(fp = fopen(buf, "r")))
                continue;
            int s = 0;
            while (fgets(buf, sizeof(buf), fp)) {
                char *p = buf;
                if (!s && (p = strstr(buf, "fastcgi.server"))) {
                    s = 1;
                    p+=15;
                }
                if (s && (p = strchr(p, '"'))) {
                    char *q;
                    if (s == 2) {
                        q = p;
                        p = buf;
                        s = 0;
                    } else if (!(q = strchr(++p, '"'))) {
                        q = p + strlen(p);
                        s = 2;
                    } else {
                        s = 0;
                    }
                    result.append(p, q - p);
                    if (!s) result.append("\n");
                }
            }
            fclose(fp);
        }
        if (dir)
            closedir(dir);

        FCGX_PutStr(result.c_str(), result.length(), cgi.out);
    }

    return 0;
}
