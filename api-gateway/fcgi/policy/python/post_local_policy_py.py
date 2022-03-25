# Copyright (C) 2020 Intel Corporation


import requests
import json
import base64
import urllib.parse
import hashlib  ## python3
import time
import random
from collections import OrderedDict
import argparse
import sys
def main(args):

    params = OrderedDict()
    params['device'] = args.device
    params['local'] = args.local # 1 for local inference 0 for remote inference

    start_time = time.time()
    ## notice: must uset http_proxy, otherwise can not get link
    url = 'http://localhost:8080/cgi-bin/fcgi_py_policy'
    res = requests.post(url,data = params)

    processing_time = time.time() - start_time


    if res.status_code == 200:
        print(res.content.decode('utf-8'))
        print("processing time is:", processing_time)
    else:
        print ("the error number is:", res.status_code)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = "it's usage tip.", description = "help info.")
    parser.add_argument("-d", "--device", choices=['CPU', 'GPU'], default = "CPU", help = "choose CPU device or GPU device")
    parser.add_argument("-l", "--local", choices=['0', '1'], default = "1", help = "choose local inference or not. 1 for local inference 0 for remote inference")

    args = parser.parse_args()
    main(args)
