# Copyright (C) 2020 Intel Corporation


import requests
import urllib.parse
from collections import OrderedDict

params = OrderedDict()
params['healthcheck'] = 1


url = 'http://localhost:8080/cgi-bin/fcgi_py_facial_landmark'
res = requests.post(url,data = params)

if res.status_code == 200:
    print(res.content.decode('utf-8'))
else:
    print ("the error number is:", res.status_code)
