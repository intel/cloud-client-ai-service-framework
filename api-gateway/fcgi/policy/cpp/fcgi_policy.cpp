// Copyright (C) 2020 Intel Corporation

// apt-get install libfcgi-dev
// gcc fcgitest.c -lfcgi

#include <stdlib.h>
#include <string.h>

#include <string>

#include <fcgiapp.h>
#include <fcgio.h>

#include <ccai_log.h>
#include <vino_ie_pipe.hpp>
#include "fcgi_utils.h"


#define LISTENSOCK_FILENO 0
#define LISTENSOCK_FLAGS 0


int main(int argc, char **argv) {
    int err = FCGX_Init(); /* call before Accept in multithreaded apps */

    if (err) {
        std::string log_info="FCGX_Init failed: " + std::to_string(err);
        CCAI_NOTICE("%s", log_info.c_str());
        return 1;
    }

    FCGX_Request cgi;
    err = FCGX_InitRequest(&cgi, LISTENSOCK_FILENO, LISTENSOCK_FLAGS);
    if (err) {
        std::string log_info = "FCGX_InitRequest failed: " + std::to_string(err);
        CCAI_NOTICE("%s", log_info.c_str());
        return 2;
    }

    while (1) {
        err = FCGX_Accept_r(&cgi);
        if (err) {
            std::string log_info = "FCGX_Accept_r stopped: " + std::to_string(err);
            CCAI_NOTICE("%s", log_info.c_str());
            break;
        }
        //-----------------------check content type------------------------------
        char *pContent = FCGX_GetParam("CONTENT_TYPE", cgi.envp);
        if ((pContent == NULL) || (strstr(pContent, "application/x-www-form-urlencoded") == NULL)) {
            CCAI_NOTICE("get content error");
            std::string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "not Acceptable";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }
        //----------------------------get the post_data form fcgi-------------------
        char *post_data = NULL;
        char *pLenstr = FCGX_GetParam("CONTENT_LENGTH", cgi.envp);
        unsigned long data_length = 0;
        if (pLenstr == NULL ||
           (data_length = strtoul(pLenstr, NULL, 10)) > INT32_MAX) {
            CCAI_NOTICE("get length error");
            std::string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "get content length error";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }

        std::string ie_result = "";
        post_data = (char *)malloc(data_length);
        if (post_data == NULL) {
            CCAI_NOTICE("malloc buffer error");
            std::string result("Status: 404 error\r\nContent-Type: text/html\r\n\r\n");
            result += "malloc buffer error";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
            continue;
        }

        FCGX_GetStr(post_data, data_length, cgi.in);
        std::string post_str = post_data;
        std::string result("Status: 200 OK\r\nContent-Type: text/html\r\ncharset: utf-8\r\n"
                           "X-Content-Type-Options: nosniff\r\nX-frame-options: deny\r\n\r\n");

        std::string sub_str = "";
        sub_str = get_data(post_str, "healthcheck");
        if (sub_str == "1") {

            result = result + "healthcheck ok";
            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
        }
        else{
            std::string local_str = "";
            local_str = get_data(post_str, "local");


            bool local = true;
            if (local_str == "1") {
                local = true;
            }
            if (local_str == "0") {
                local = false;
            }

            std::string device_str = "";
            device_str = get_data(post_str, "device");

            struct userCfgParams cfg{local, device_str};
            int res_policy = vino_ie_pipeline_set_parameters(cfg);

            if (res_policy == -1) {
                result += "failed to set policy daemon";
            } else {
                result += "successfully set the policy daemon";
            }

            //-----------------------------prepare output -----------------------------

            FCGX_PutStr(result.c_str(), result.length(), cgi.out);
        }
        //------------------free memory----------------------

        free(post_data);

    }

    return 0;
}
