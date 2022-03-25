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
from tencentcloud.asr.v20190614 import asr_client, models
import time

def get_result(environ):
    start_time = time.time()
    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0
        print ('ValueError')
    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())
    audio_data = d[b'speech']

    cred = credential.Credential("AKIDcv8JE0kNxGqDTYO3oKZKqhJzTmupuBXe", "4sUu7B1YsxlTyEMKK9nB3mIgpHWXNTM0")
    httpProfile = HttpProfile()
    httpProfile.endpoint = "asr.tencentcloudapi.com"
    clientProfile = ClientProfile()
    clientProfile.httpProfile = httpProfile
    client = asr_client.AsrClient(cred, "ap-beijing", clientProfile)
    req = models.CreateRecTaskRequest()

    params = '{"EngineModelType": "8k_0", "ChannelNum": 1, "ResTextFormat": 0, "SourceType": 1, "DataLen":' + str(len(audio_data)) + ', "Data":"' + str(audio_data, 'utf-8') + '"}'
    req.from_json_string(params)
    resp = client.CreateRecTask(req)
    result_text = resp.to_json_string()
    result_dict = json.loads(result_text)
    task_id = result_dict['Data']['TaskId']

    req2 = models.DescribeTaskStatusRequest()
    params = '{"TaskId":' + str(task_id) + '}'
    req2.from_json_string(params)
    resp2 = client.DescribeTaskStatus(req2)
    recognition_text = resp2.to_json_string()
    result_dict = json.loads(recognition_text)

    status = 0
    while status == 0 or status == 1:
        resp2 = client.DescribeTaskStatus(req2)
        recognition_text = resp2.to_json_string()
        result_dict = json.loads(recognition_text)
        status = result_dict["Data"]['Status']
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
