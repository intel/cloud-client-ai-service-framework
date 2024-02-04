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

    appkey = 'user-key'

    f = open(args.image, "rb")
    base64_data = base64.b64encode(f.read())
    time_stamp = int(time.time())
    random_let = ''.join(random.sample('zyxwvutsrqponmlkjihgfedcba1234567890', 10))

    params = OrderedDict()
    params['app_id'] = user-id
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
    url= 'http://localhost:8080/cgi-bin/fcgi_py_segmentation'
    res = requests.post(url,data=params)

    processing_time = time.time() - start_time
    image_width = 513
    image_height = 513

    if res.status_code == 200:
        print(res.content.decode('utf-8'))
        result_dict = json.loads(res.text, strict = False)
        if result_dict['ret'] == 0:
            img_str =  base64.b64decode(bytes(result_dict['data'][1:-1], encoding = "utf8"))

            colors = {0:[128,64,128],
                      1:[232,35,244],
                      2:[70,70,70],
                      3:[156,102,102],
                      4:[153,153,190],
                      5:[153,153,153],
                      6:[30,170,250],
                      7:[0,220,220],
                      8:[35,142,107],
                      9:[153,251,152],
                      10:[180,130,70],
                      11:[180,130,70],
                      12:[60,20,220],
                      13:[0,0,255],
                      14:[142,0,0],
                      15:[70,0,0],
                      16:[100,60,0],
                      17:[90,0,0],
                      18:[230,0,0],
                      19:[32,11,119],
                      20:[0,74,111],
                      21:[81,0,81]
            }
            img_mat=np.zeros((image_height, image_width, 3))
            for h in range(image_height):
                for w in range(image_width):
                    if(img_str[image_width * h + w] in colors.keys()):
                        img_mat[h][w] = colors[img_str[image_width * h + w]]

            cv2.imwrite('out.jpg', img_mat)
            print ("the sagementation image is saved in out.jpg")
            print ("processing time is:", processing_time)

    else:
        print ("the error number is:", res.status_code)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(usage = "it's usage tip.", description = "help info.")
    parser.add_argument("-i", "--image", default = "./input_data/face-detection-adas-0001.png", help = "the path of the picture")

    args = parser.parse_args()
    main(args)
