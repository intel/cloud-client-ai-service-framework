# Copyright (C) 2020 Intel Corporation

#
#NOTE: Please read README.txt before enabling this case
#
import requests
import json
import base64
import urllib.parse
import hashlib 
import time
import random
import wave
import numpy as np
from collections import OrderedDict

params = OrderedDict()
params['mode'] = 0
query_string = urllib.parse.urlencode(params)
m = hashlib.md5(query_string.encode())
sign = m.hexdigest().upper()
params['sign'] = sign
start_time = time.time()

url = 'http://localhost:8080/cgi-bin/fcgi_live_asr'
res = requests.post(url,data = params)

processing_time = time.time() - start_time

if res.status_code == 200:
    result_dict = json.loads(res.text)
    if int(result_dict['ret']) == 1:
        print(result_dict['msg'])
        exit(0)
    else:
        print(result_dict['data']['text'])
else:
    print ("the error number is:", res.status_code)
    exit(0)

time.sleep(3)

params['mode'] = 1
for i in range(30):
    res = requests.post(url,data = params)
    if res.status_code == 200:
        result_dict = json.loads(res.text)
        print(result_dict['data']['text'])
    else:
        print ("the error number is:", res.status_code)

params['mode'] = 2
res = requests.post(url,data = params)
if res.status_code == 200:
    result_dict = json.loads(res.text)
    print(result_dict['data']['text'])
else:
    print ("the error number is:", res.status_code)

