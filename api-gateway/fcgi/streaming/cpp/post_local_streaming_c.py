# Copyright (C) 2021 Intel Corporation


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
    start_payload = {
                'pipeline':'launcher.pose_estimation',
                'method':'start',
                'parameter':{
                               'source':'device=/dev/video0',
                               'sink':'fpsdisplaysink video-sink=ximagesink sync=false',
                               'framerate':'inference-interval=1'
                             }
               }
    header = {'Content-Type':'application/json'}
    url = 'http://localhost:8080/cgi-bin/streaming'
    res = requests.post(url, headers = header, data = json.dumps(start_payload))
    if res.status_code != 200:
        print("error! start error number is:", res.status_code)
        exit(0)
    print("start: ", res.text)    

    read_payload = {
                'pipeline':'launcher.pose_estimation',
                'method':'read',
               }
    for _ in range(60):
        time.sleep(1)

        res = requests.post(url, headers = header, data = json.dumps(read_payload))
        if res.status_code != 200:
            print("error! stop error number is:", res.status_code)
            exit(0)
        print("read: ", res.text)    
        #status = json.loads(res.text)

    stop_payload = {
                'pipeline':'launcher.pose_estimation',
                'method':'stop',
               }

    res = requests.post(url, headers = header, data = json.dumps(stop_payload))
    if res.status_code != 200:
        print("error! stop error number is:", res.status_code)
    print("stop: ", res.text)    

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = "it's usage tip.", description = "help info.")

    args = parser.parse_args()
    main(args)
