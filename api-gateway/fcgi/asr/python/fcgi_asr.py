#!/usr/bin/python3
# Copyright (C) 2020 Intel Corporation

from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer

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
syslog = logging.handlers.SysLogHandler(address='/dev/log')

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

    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    app_id = d.get(b'app_id')
    rate = d.get(b'rate')
    audio_format = d.get(b'format')
    audio_base64 = d.get(b'speech')

    logger.info("AI-Service-Framework: asr python service")
    if audio_base64 == None:
        logger.info('AI-Service-Framework: cannot get audio data')

    audio_data = base64.b64decode(audio_base64)

    speech = np.fromstring(audio_data, dtype = np.short)
    sampwidth = d.get(b'nonce_str')

    model_xml = './models/lspeech_s5_ext/FP32/speech_lib.cfg'
    buf = np.zeros((100 * 100), dtype = np.int8)
    utt_res = rt_api.vectorChar(buf)
    urlinfo = rt_api.serverParams()
    urlinfo.url = 'https://api.ai.qq.com/fcgi-bin/aai/aai_asr'
    urlinfo.urlParam = data

    res = rt_api.infer_speech(speech, int(sampwidth), model_xml, "OPENVINO", utt_res, urlinfo)
    if res == 0:
        logger.info('AI-Service-Framework: local inference')
        str1 = ''.join(utt_res)
        ie_dict = {}
        ie_dict["text"] = str1.split('\u0000')[0]
        #str1[ret] = '0'
        result_dict = {}
        result_dict["ret"] = 0
        result_dict["msg"] = "ok"
        result_dict["data"] = ie_dict

        server_running_time = round(time.time() - start_time, 3)
        result_dict["time"] = server_running_time
        result_json = json.dumps(result_dict, indent = 1)
    elif res == 1:
        logger.info('AI-Service-Framework: remote inference')
        result_text = ("{" + urlinfo.response.split("{", 1)[1])
        result_dict = json.loads(result_text)
        server_running_time = round(time.time() - start_time, 3)
        result_dict['time'] = server_running_time
        result_json = json.dumps(result_dict, indent = 1)
        result_json = str(result_json).encode('utf-8').decode('unicode_escape').encode('utf-8')

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

