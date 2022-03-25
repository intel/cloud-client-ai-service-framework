// Copyright (C) 2020 Intel Corporation
//

#include <chrono> 
#include <iostream>
#include <string>

#include <syslog.h>

#include "curl/curl.h"  
#include "connect_server.hpp"

#define NO_VERIFICATION  1

// reply of the requery  
size_t HttpRequest::req_reply(void *ptr, size_t size, size_t nmemb, void *stream) {

    std::string *str = reinterpret_cast<std::string*>(stream);  
    (*str).append(reinterpret_cast<char*>(ptr), size * nmemb);

    return size * nmemb;
}

// http POST  
CURLcode HttpRequest::curl_post_req(const std::string &url,
                                    const char *postParams, std::string &response) {
    // init curl
    CURL *curl = curl_easy_init();
    // res code
    CURLcode res = CURLE_OK;
    if (curl) {
        // set params
        curl_easy_setopt(curl, CURLOPT_POST, 1); // post req
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        /* used for SSL verify, but the place of certification should be set the correct value */
    #ifndef NO_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_CAPATH,"/openvino_model/open_model_zoo/demos/ie_common_api/models");
        curl_easy_setopt(curl, CURLOPT_CAINFO,"./models/ca_info.pem");
        curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
        curl_easy_setopt(curl, CURLOPT_SSLCERT, "./models/client.pem");
        curl_easy_setopt(curl, CURLOPT_SSLKEY, "./models/client_key.pem");
        curl_easy_setopt(curl, CURLOPT_KEYPASSWD, "your_key_password");
    #endif
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postParams);
    #ifdef NO_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0); // if want to use https
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0); // set peer and host verify false
    #endif
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, req_reply);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_HEADER, 1);

        //curl_easy_setopt(curl, CURLOPT_PROXY, "https://child-prc.intel.com:913/");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L); //connection timeout 60s
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 100L); //Timeout is 100s

        // start req
        res = curl_easy_perform(curl);
    }
    // release curl
    curl_easy_cleanup(curl);

    return res;
}


int HttpRequest::HttpPost(const std::string &url,
                          const char *param, std::string &response) {

    if (!param) {
        syslog(LOG_ERR, "AI-Service-Framework: ERROR: no post parameter!");
        return -1;
    }

    // global init  
    curl_global_init(CURL_GLOBAL_ALL);

    int err = 1;
    auto res = curl_post_req(url, param, response);
    if (res != CURLE_OK) {
        syslog(LOG_ERR, "AI-Service-Framework: curl_easy_perform() failed: %s",
               std::string(curl_easy_strerror(res)).c_str());
        err = -1;
    }
    else
        std::cout << "response OK" << std::endl;

    // global release
    curl_global_cleanup();

    return err;
}

int HttpRequest::HttpGet(const std::string &url) {

    CURL *curl;
    CURLcode res = CURLE_OK;
    std::string response;
 
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
 
    #ifdef NO_VERIFICATION
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    #endif
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, req_reply);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_HEADER, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L); //connection timeout is 60s
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 100L);
 
        res = curl_easy_perform(curl);
        if(res != CURLE_OK)
            syslog(LOG_ERR, "AI-Service-Framework: curl_easy_perform() failed: %s\n",
                   curl_easy_strerror(res));
        else
            std::cout << response << std::endl;
 
        /* cleanup */
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    return 1;
}
