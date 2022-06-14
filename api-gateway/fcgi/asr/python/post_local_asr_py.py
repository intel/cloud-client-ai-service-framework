# Copyright (C) 2022 Intel Corporation

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

    policy_params = OrderedDict()
    policy_params['device'] = "CPU"
    policy_params['local'] = "1" # 1 for local inference 0 for remote inference
    url = 'http://localhost:8080/cgi-bin/fcgi_py_policy'
    res = requests.post(url, data = policy_params)
    if res.status_code != 200:
        print("set device error! error number is:", res.status_code)

    appkey = 'di6ik9b9JiYfImUB'

    if args.wave_file == None:
        if args.language == 'ENG':
            args.wave_file = './input_data/4507-16021-0012_en.wav'
        elif args.language == 'CHN':
            args.wave_file = './input_data/D4_750_ch.wav'
        else:
            print("language parameter error!")
            exit(0)

    f = wave.open(args.wave_file, "rb")
    params = f.getparams()
    nchannels, sampwidth, framerate, nframes = params[:4]
    str_data = f.readframes(nframes)
    f.close()

    speech = np.frombuffer(str_data, dtype = np.short)
    base64_data = base64.b64encode(str_data)

    time_stamp = int(time.time())
    random_let = ''.join(random.sample('zyxwvutsrqponmlkjihgfedcba1234567890', 10))

    params = OrderedDict()
    params['format'] = 2
    params['samplewidth'] = sampwidth
    params['rate'] = 16000
    params['speech'] = base64_data

    if args.language == 'ENG':
        params['language'] = 'ENGLISH'
    elif args.language == 'CHN':
        params['language'] = 'CHINESE'
    else:
        print("language parameter error!")
        exit(0)

    if args.mode == 'Offline':
        params['realtime'] = 'OFFLINE'
    elif args.mode == 'Online':
        params['realtime'] = 'ONLINE_READ'
    else:
        print("mode parameter error!")
        exit(0)

    if args.audio_input == 'SIMULATION' or args.audio_input == 'MIC':
        params['audio_input'] = args.audio_input
    else:
        print("audio_input parameter error!")
        exit(0)

    params['time_stamp'] = time_stamp
    query_string = urllib.parse.urlencode(params)
    query_string += '&app_key=' + appkey
    m = hashlib.md5(query_string.encode())
    sign = m.hexdigest().upper()
    params['sign'] = sign
    start_time = time.time()

    url = 'http://localhost:8080/cgi-bin/fcgi_py_asr'

    if params['realtime'] == 'ONLINE_READ':
        for i in range(10):
            res = requests.post(url,data = params)

            if res.status_code == 200:
                text = res.content.decode('utf-8')
                text = json.loads(text)
                #print(text)
                if text['data']['text'] != '':
                    print(text['data']['text'])
            else:
                print ("the error number is:", res.status_code)

        params['realtime'] = 'ONLINE_STOP'
        res = requests.post(url,data = params)
        if res.status_code == 200:
            text = res.content.decode('utf-8')
            text = json.loads(text)
            print(text['data']['text'])
        else:
            print ("the error number is:", res.status_code)
    else:
        res = requests.post(url,data = params)
        processing_time = time.time() - start_time

        if res.status_code == 200:
            text = res.content.decode('utf-8')
            text = json.loads(text)
            #print(text)
            print(text['data']['text'])
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
    parser.add_argument("-l", "--language", type=str, required = True, help = "ENG(english) or CHN(chinese)")
    parser.add_argument("-m", "--mode", type=str, required = True, help = "Offline or Online")
    parser.add_argument("-f", "--wave_file", type=str, help = "the path of the wave file")
    parser.add_argument("-i", "--audio_input", type=str, default="SIMULATION", help = "The audio input in online mode: SIMULATION or MIC")

    args = parser.parse_args()
    main(args)
