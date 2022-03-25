#!/usr/bin/python3
# Copyright (C) 2020 Intel Corporation

from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer

import json
import base64
import os
import time
import sys
import wave
import datetime
import numpy as np
import ctypes
import inferservice_python as rt_api
import cv2

import logging
import logging.handlers
import socket

syslog = logging.handlers.SysLogHandler(address='/dev/log')
msgfmt = '%(asctime)s {0} %(name)s[%(process)d]: %(message)s'.format(socket.gethostname())
formatter = logging.Formatter(msgfmt, datefmt='%b %d %H:%M:%S')
syslog.setFormatter(formatter)

logger = logging.getLogger(os.path.basename(sys.argv[0]))
logger.addHandler(syslog)
logger.setLevel(logging.DEBUG)

def get_result(environ):

    start_time = time.time()

    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    model_xml = "./models/face-detection-adas-0001.xml"

    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    app_id = d.get(b'app_id')
    image_base64 = d.get(b'image')

    logger.info("AI-Service-Framework: facial_landmark python service")
    if image_base64 == None:
        logger.info('AI-Service-Framework: cannot get image data')

    image_data = base64.b64decode(image_base64)
    image = cv2.imdecode(np.fromstring(image_data, np.uint8), cv2.IMREAD_COLOR)
    pic = list(image_data)
    pics = [pic]

    other_pin = rt_api.vectorVecFloat()

    out1 = rt_api.vectorFloat()
    out = rt_api.vectorVecFloat()
    out.append(out1)

    urlinfo = rt_api.serverParams()
    urlinfo.url = 'https://api.ai.qq.com/fcgi-bin/face/face_faceshape'
    urlinfo.urlParam = data

    res = rt_api.infer_image(pics, 3, other_pin, model_xml, out, urlinfo)
    if res == 0:
        logger.info('AI-Service-Framework: local inference')
        maxProposalCount = 200
        objectSize = 7
        bb_enlarge_coefficient = 1.2
        detectionThreshold = 0.5
        bb_dx_coefficient = 1
        bb_dy_coefficient = 1

        ie_list = []
        facial_pics =[]

        for i in range(maxProposalCount):
            image_id = out[0][i * objectSize]
            if image_id < 0 :
                break
            label = out[0][i * objectSize + 1]
            confidence = out[0][i * objectSize + 2]
            if confidence < detectionThreshold:
                continue

            location_x = out[0][i * objectSize + 3] * image.shape[1]
            location_y = out[0][i * objectSize + 4] * image.shape[0]

            location_width = out[0][i * objectSize + 5] * image.shape[1] - location_x
            location_height = out[0][i * objectSize + 6] * image.shape[0] - location_y
            center_x = location_x + location_width / 2
            center_y = location_y + location_height / 2

            max_of_sizes = location_width if location_width > location_height else location_height
            location_width = bb_enlarge_coefficient * max_of_sizes
            location_height = bb_enlarge_coefficient * max_of_sizes
            location_x = center_x - (bb_dx_coefficient * location_width / 2)
            location_y = center_y - (bb_dy_coefficient * location_height / 2)

            ie_dict = {}
            if confidence > detectionThreshold :
                ie_dict["x"] = int(location_x)
                ie_dict["y"] = int(location_y)
                ie_dict["width"] = int(location_width)
                ie_dict["height"] = int(location_height)
                ie_list.append(ie_dict)

        for ie_dict in ie_list:
            crop_image = image[ie_dict["y"]:ie_dict["y"] + ie_dict["height"], ie_dict["x"]:ie_dict["x"] + ie_dict["width"]]
            success, encoded_image = cv2.imencode(".jpg", crop_image)

            facial_pic = list(encoded_image)
            facial_pics.append(facial_pic)

        model_xml = './models/facial-landmarks-35-adas-0002.xml'
        model_bin = os.path.splitext(model_xml)[0] + ".bin"
        facial_out1 = rt_api.vectorFloat()
        facial_out = rt_api.vectorVecFloat()
        facial_out.append(facial_out1)
        res = rt_api.infer_image(facial_pics, 3, other_pin, model_xml, facial_out, urlinfo)

        n_lm = 70
        facial_list = []
        for i in range(len(facial_pics)):
            begin =int(n_lm * i / 2)
            end =int(begin + n_lm /2)
            for j in range(begin, end):
                normed_x = facial_out[0][2 * j]
                normed_y = facial_out[0][2 * j + 1]
                normed_x = normed_x * ie_list[i]['width'] + ie_list[i]['x']
                normed_y = normed_y * ie_list[i]['width'] + ie_list[i]['y']

                point_dict = {}
                point_dict["x"] = normed_x
                point_dict["y"] = normed_y

                facial_list.append(point_dict)

        facial_dict = {}
        facial_dict["image_width"] = image.shape[1]
        facial_dict["image_height"] = image.shape[0]
        facial_dict["face_shape_list"] = facial_list

        result_dict = {}
        result_dict["ret"] = 0
        result_dict["msg"] = "ok"
        result_dict["data"] = facial_dict

        server_running_time = round(time.time() - start_time, 3)
        result_dict["time"] = server_running_time
        result_json = json.dumps(result_dict, indent = 1)

    elif res == 1:
        logger.info('AI-Service-Framework: remote inference')
        result_text = ("{" + urlinfo.response.split("{", 1)[1])
        result_dict = json.loads(result_text)
        server_running_time = round(time.time() - start_time, 3)
        result_dict['time'] = server_running_time
        result_json = json.dumps(result_dict, indent = 1)
        result_json = str(result_json).encode('utf-8').decode('unicode_escape').encode('utf-8')

    else:
        result_dict = {}
        result_dict["ret"] = 1
        result_dict["msg"] = "inference failed"
        result_json = json.dumps(result_dict, indent = 1)
        logger.info('AI-Service-Framework: can not get inference results')

    return result_json

def application(environ, start_response):


    result = get_result(environ)
    body = result
    status = '200 OK'
    headers = [('Content-Type', 'text/plain')]
    start_response(status, headers)

    return [body]

if __name__ == '__main__':
    WSGIServer(application).run()
