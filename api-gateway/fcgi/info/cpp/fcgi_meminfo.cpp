// Copyright (C) 2020 Intel Corporation


// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi
#include <syslog.h>
#include <string>
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

        char buf[1024];
        size_t len;
        FILE* fp = fopen("/proc/meminfo", "r");
        while((len = fread(buf, 1, sizeof(buf), fp))) {
            result.append(buf, len);
        }
        fclose(fp);

        FCGX_PutStr(result.c_str(), result.length(), cgi.out);
    }

    return 0;
}
