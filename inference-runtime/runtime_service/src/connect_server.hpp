// Copyright (C) 2020 Intel Corporation
//

#pragma once

#include <string>
#include "curl/curl.h"

class HttpRequest
{
public:
    HttpRequest(){ };
    ~HttpRequest(){ };
    int HttpPost(const std::string &url, const char* param, std::string &response);
    int HttpGet(const std::string &url);

private:
    static size_t req_reply(void *ptr, size_t size, size_t nmemb, void *stream);
    CURLcode curl_post_req(const std::string &url, const char *postParams, std::string &response);
};
