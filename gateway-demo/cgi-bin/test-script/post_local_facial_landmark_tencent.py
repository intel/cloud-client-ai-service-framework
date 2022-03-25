# Copyright (C) 2018-2020 Intel Corporation


import requests
import json
import base64
import urllib.parse
import hashlib  ## python3
import time
import random
from collections import OrderedDict

appkey = 'N05YjTsH5RS5S9bI'

f = open("./models/face-detection-adas-0001.png","rb")
base64_data = base64.b64encode(f.read())
time_stamp = int(time.time())
random_let = ''.join(random.sample('zyxwvutsrqponmlkjihgfedcba1234567890',10))

params = OrderedDict()
params['app_id'] = 2128571502
params['sign'] = ''
params['nonce_str'] = random_let
params['mode'] = 1
params['image'] = base64_data
params['time_stamp'] = time_stamp
#print(params)

start_time = time.time()

## notice: must uset http_proxy, otherwise can not get link
url= 'http://localhost:8080/cgi-bin/fcgi_py_tencent_facial_landmark'
res = requests.post(url,data=params)


processing_time = time.time() - start_time

print(res.content)
print("processing time is:", processing_time)
