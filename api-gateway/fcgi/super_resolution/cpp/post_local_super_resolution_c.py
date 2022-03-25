# Copyright (C) 2020 Intel Corporation


import requests
import json
import base64
import urllib.parse
import hashlib  ## python3
import time
import random
import cv2
from collections import OrderedDict
import numpy as np
import argparse
import sys

def main(args):
    #start_time = time.time()

    appkey = 'di6ik9b9JiYfImUB'

    f = open(args.image, "rb")
    base64_data = base64.b64encode(f.read())
    time_stamp = int(time.time())
    random_let = ''.join(random.sample('zyxwvutsrqponmlkjihgfedcba1234567890', 10))

    params = OrderedDict()
    params['app_id'] = 2128571502
    params['image'] = base64_data
    params['nonce_str'] =time_stamp
    params['time_stamp'] = time_stamp
    query_string = urllib.parse.urlencode(params)
    query_string += '&app_key=' + appkey
    m = hashlib.md5(query_string.encode())
    sign = m.hexdigest().upper()
    params['sign'] = sign


    start_time = time.time()

    ## notice: must uset http_proxy, otherwise can not get link
    #url = 'https://api.ai.qq.com/fcgi-bin/aai/aai_asr';
    url = 'http://localhost:8080/cgi-bin/fcgi_super_resolution'
    res = requests.post(url,data=params)

    processing_time = time.time() - start_time
    image_width = 1920
    image_height = 1080
    image_channel = 3
    if res.status_code == 200:
        print(res.content.decode('utf-8'))
        result_dict = json.loads(res.text, strict = False)
        if result_dict['ret'] == 0:
            img_str =  base64.b64decode(bytes(result_dict['data'], encoding = "utf8"))
            img_np = np.frombuffer(img_str, dtype=np.uint8)
            img =np.array(img_np[0: image_height * image_width * image_channel]).reshape(image_height, image_width, image_channel)
            cv2.imwrite('out.jpg', img)
            print ("the sagementation image is saved in out.jpg")
            print ("processing time is:", processing_time)
    else:
        print ("the error number is:", res.status_code)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = "it's usage tip.", description = "help info.")
    parser.add_argument("-i", "--image", default = "./input_data/face-detection-adas-0001.png", help = "the path of the picture")

    args = parser.parse_args()
    main(args)
