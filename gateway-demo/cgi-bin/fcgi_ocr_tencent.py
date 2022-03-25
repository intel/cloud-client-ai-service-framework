#!/usr/bin/python3
# Copyright (C) 2018-2020 Intel Corporation



from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer

import json
import base64
import os, sys
import time
import numpy as np
from tencentcloud.common import credential
from tencentcloud.common.profile.client_profile import ClientProfile
from tencentcloud.common.profile.http_profile import HttpProfile
from tencentcloud.common.exception.tencent_cloud_sdk_exception import TencentCloudSDKException
from tencentcloud.ocr.v20181119 import ocr_client, models


def get_result(environ):
    start_time = time.time()
    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0
        print ('ValueError')
    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())
    audio_data = d[b'image']
    cred = credential.Credential("AKIDcv8JE0kNxGqDTYO3oKZKqhJzTmupuBXe", "4sUu7B1YsxlTyEMKK9nB3mIgpHWXNTM0")
    httpProfile = HttpProfile()
    httpProfile.endpoint = "ocr.tencentcloudapi.com"
    clientProfile = ClientProfile()
    clientProfile.httpProfile = httpProfile
    client = ocr_client.OcrClient(cred, "ap-beijing", clientProfile)

    req = models.GeneralBasicOCRRequest()
    params = '{"ImageBase64":"' + str(audio_data, 'utf-8') + '"}'
    req.from_json_string(params)
    resp = client.GeneralBasicOCR(req)
    result_text = resp.to_json_string()
    result_dict = json.loads(result_text)

    server_running_time = round(time.time() - start_time, 3)
    result_dict['time'] = server_running_time

    result_json = json.dumps(result_dict)
    str1 = str(result_json).encode('utf-8').decode('unicode_escape').encode('utf-8')
    return str1


def application(environ, start_response):


    result = get_result(environ)
    body = result
    status = '200 OK'
    headers = [('Content-Type', 'text/plain')]
    start_response(status, headers)

    return [body]

if __name__ == '__main__':
    WSGIServer(application).run()
