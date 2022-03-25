# Copyright (C) 2020 Intel Corporation


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
import argparse
import sys

def main(args):

    # set inference device to GNA_AUTO
    policy_params = OrderedDict()
    policy_params['device'] = args.device
    policy_params['local'] = "1" # 1 for local inference 0 for remote inference
    url = 'http://localhost:8080/cgi-bin/fcgi_py_policy'
    res = requests.post(url, data = policy_params)
    if res.status_code != 200:
        print("set device error! error number is:", res.status_code)

    appkey = 'di6ik9b9JiYfImUB'

    f = wave.open(args.audio, "rb")
    params = f.getparams()
    nchannels, sampwidth, framerate, nframes = params[:4]
    str_data = f.readframes(nframes)
    f.close()

    speech = np.fromstring(str_data, dtype = np.short)

    base64_data = base64.b64encode(str_data)

    time_stamp = int(time.time())
    random_let = ''.join(random.sample('zyxwvutsrqponmlkjihgfedcba1234567890', 10))

    params = OrderedDict()
    params['app_id'] = 2128571502
    params['format'] = 2
    params['nonce_str'] = sampwidth
    params['rate'] = 16000
    params['speech'] = base64_data
    params['time_stamp'] = time_stamp
    query_string = urllib.parse.urlencode(params)
    query_string += '&app_key=' + appkey
    m = hashlib.md5(query_string.encode())
    sign = m.hexdigest().upper()
    params['sign'] = sign
    start_time = time.time()

    url = 'http://localhost:8080/cgi-bin/fcgi_py_asr'
    res = requests.post(url,data = params)

    processing_time = time.time() - start_time

    if res.status_code == 200:
        print(res.content.decode('utf-8'))
        print("processing time is:", processing_time)
    else:
        print ("the error number is:", res.status_code)

    # set back default inference device "CPU"
    policy_params['device'] = "CPU"
    policy_params['local'] = "1" # 1 for local inference 0 for remote inference
    url = 'http://localhost:8080/cgi-bin/fcgi_py_policy'
    res = requests.post(url, data = policy_params)
    if res.status_code != 200:
        print("set device error! error number is:", res.status_code)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = "it's usage tip.", description = "help info.")
    parser.add_argument("-a", "--audio", default = "./input_data/how_are_you_doing.wav", help = "the path of the audio")
    parser.add_argument("-d", "--device", default = "GNA_AUTO", help = "choose inference device: CPU, GNA_AUTO")

    args = parser.parse_args()
    main(args)
