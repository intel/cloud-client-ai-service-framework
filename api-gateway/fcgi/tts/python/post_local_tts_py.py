# Copyright (C) 2020 Intel Corporation

#
#NOTE: Please read README.txt before enabling this case
#
import requests
import json
import base64
import urllib.parse
import hashlib  ## python3
import time
import random
from collections import OrderedDict
import numpy as np
from scipy.io.wavfile import write
import os
import argparse
import sys

def main(args):

    #start_time = time.time()

    appkey = 'di6ik9b9JiYfImUB'
    f = open(args.sentence, "r")

    time_stamp = int(time.time())
    random_let = ''.join(random.sample('zyxwvutsrqponmlkjihgfedcba1234567890', 10))

    params = OrderedDict()
    params['aht'] = 0
    params['apc'] = 58
    params['app_id'] = 2128571502
    params['format'] = 2
    params['nonce_str'] = time_stamp
    params['speaker'] = 1
    params['speed'] = 100

    params['text'] = f.read()

    params['time_stamp'] = time_stamp
    params['volume'] = 0
    query_string = urllib.parse.urlencode(params)
    query_string += '&app_key=' + appkey
    m = hashlib.md5(query_string.encode())
    sign = m.hexdigest().upper()
    params['sign'] = sign


    start_time = time.time()
    url = 'http://localhost:8080/cgi-bin/fcgi_py_tts'

    res = requests.post(url, data = params)

    if res.status_code == 200:
        print (res.text)
        result_dict = json.loads(res.text)
        text = base64.b64decode(bytes(result_dict['data']['speech'], encoding = "utf8"))
        with open("./1.wav", "wb") as f:
            f.write(text)

        processing_time = time.time() - start_time
        print(result_dict['msg'])
        print("processing time is:", processing_time)
    else:
        print ("the error number is:", res.status_code)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = "it's usage tip.", description = "help info.")
    parser.add_argument("-s", "--sentence", default = "./input_data/test_sentence.txt", help = "the path of the text")

    args = parser.parse_args()
    main(args)
