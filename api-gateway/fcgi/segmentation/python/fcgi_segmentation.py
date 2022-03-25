#!/usr/bin/python3
# Copyright (C) 2020 Intel Corporation

from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer
import subprocess
import json
import base64
import os
import time
import sys
import wave
import datetime
import numpy as np
import ctypes
import inferservice_python as rt_api


import logging
import logging.handlers
import socket

syslog = logging.handlers.SysLogHandler(address=('localhost', 514), facility='user', socktype=socket.SOCK_DGRAM)
msgfmt = '%(asctime)s {0} %(name)s[%(process)d]: %(message)s'.format(socket.gethostname())
formatter = logging.Formatter(msgfmt, datefmt='%b %d %H:%M:%S')
syslog.setFormatter(formatter)

logger = logging.getLogger(os.path.basename(sys.argv[0]))
logger.addHandler(syslog)
logger.setLevel(logging.DEBUG)


def get_result(environ):

    start_time = time.time()

    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    model_xml="./models/deeplab_fp32/frozen_inference_graph.xml"


    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    app_id = d.get(b'app_id')

    image_base64 = d.get(b'image')
    if image_base64 == None:
        logger.info('AI-Service-Framework: cannot get image data')
    image_data = base64.b64decode(image_base64)
    pic = list(image_data)
    pics = [pic]

    other_pin = rt_api.vectorVecFloat()

    out1 = rt_api.vectorFloat()
    out = rt_api.vectorVecFloat() ##with opaque and stl_binding
    out.append(out1)

    urlinfo = rt_api.serverParams()
    urlinfo.url = 'https://api.ai.qq.com/fcgi-bin/image/image_tag'
    urlinfo.urlParam = data
    res = rt_api.infer_image(pics, 3, other_pin, model_xml, out, urlinfo)

    if res == 0:
        logger.info('AI-Service-Framework: local inference')

        result_dict = {}
        result_dict["ret"] = 0
        result_dict["msg"] = "ok"
        image = base64.b64encode(np.array(out[0]))
        result_dict["data"] = str(image)
        server_running_time = round(time.time() - start_time, 3)
        result_dict["time"] = server_running_time

        result_json = json.dumps(result_dict, indent = 1)

    elif res == 1:

        logger.info('AI-Service-Framework: remote inference')
        result_dict = {}
        result_dict["ret"] = 2
        result_dict["msg"] = "no remote inference"
        result_json = json.dumps(result_dict, indent = 1)

    else:
        result_dict = {}
        result_dict["ret"] = 1
        result_dict["msg"] = "inference failed"
        result_json = json.dumps(result_dict, indent = 1)
        logger.info('AI-Service-Framework: can not get inference results')
    return result_json

def application(environ, start_response):


    result = get_result(environ)
    body = result
    status = '200 OK'
    headers = [('Content-Type', 'text/plain')]
    start_response(status, headers)

    return [body]

if __name__ == '__main__':
    WSGIServer(application).run()
