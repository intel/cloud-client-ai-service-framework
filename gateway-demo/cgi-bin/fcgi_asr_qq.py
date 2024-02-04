#!/usr/bin/python3
# Copyright (C) 2018-2020 Intel Corporation



from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer

import json
import base64
import os,sys
import time
import numpy as np
import time

import requests
import urllib.parse
import hashlib 
import random
from collections import OrderedDict


def get_result(environ):
    start_time = time.time()
    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0
        print ('ValueError')
    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k,v[0]) for k,v in parse_qs(data).items())
    audio_data = d[b'speech']

    appkey = 'user-key'
    time_stamp = int(time.time())
    random_let = ''.join(random.sample('zyxwvutsrqponmlkjihgfedcba1234567890',10))
    params = OrderedDict()
    params['app_id'] = user-id
    params['format'] = 2
    params['nonce_str'] = random_let
    params['rate'] = 16000
    params['speech'] = audio_data
    params['time_stamp'] = time_stamp
    query_string = urllib.parse.urlencode(params)
    query_string += '&app_key=' + appkey
    m = hashlib.md5(query_string.encode())
    sign = m.hexdigest().upper()
    params['sign'] = sign
    url = 'https://api.ai.qq.com/fcgi-bin/aai/aai_asr';
    res = requests.post(url,data=params)
    processing_time = round(time.time() - start_time,3)
    str1 = res.content.decode()[0:-3]+',\n\"time\":'+str(processing_time)+'\n}\n'
    str1=str1.encode()
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
