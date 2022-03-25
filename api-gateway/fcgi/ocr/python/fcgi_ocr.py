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
import math

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

def softmax(x):

    x =x.flatten()

    for i in range(0, len(x), 2):
        if x[i] > x[i + 1]:
            max_x = x[i]
        else:
            max_x = x[i + 1]
        x[i] = math.exp(x[i] - max_x)
        x[i + 1] = math.exp(x[i + 1] - max_x)
        sum_x = x[i] + x[i + 1]
        x[i] = x[i] / sum_x
        x[i + 1] = x[i + 1] / sum_x
    return x

def sliceAndGetSecondChannel(data):
    new_data = []
    for i in range(int (len(data) / 2)):
        new_data.append( data[2 * i + 1])
    return new_data

def findRoot(point, group_mask):
    root = point
    update_parent = 0
    while group_mask[root] != -1 :
        root = group_mask[root]
        update_parent = 1
    if update_parent :
        group_mask[point] = root

    return root

def join(p1, p2, group_mask):
    root1 = findRoot(p1, group_mask)
    root2 = findRoot(p2, group_mask)
    if(root1 != root2):
        group_mask[root1] = root2

def get_all(points, w, h, group_mask):
    root_map =  {}
    mask = np.zeros([h, w])
    for point in points:
        point_root = findRoot(point[0] + point[1] * w, group_mask)

        if point_root not in root_map.keys():
            root_map[point_root] = len (root_map) + 1

        mask[point[1]][point[0]] = root_map[point_root]

    return mask

def decodeImageByJoin(link_data, cls_data, link_data_shape, cls_data_shape, link_conf_threshold, cls_conf_threshold):

    w = cls_data_shape[2]
    h = cls_data_shape[1]
    pixel_mask_shape = (h * w, 0)

    points = []
    group_mask = [0 for x in range(0, h * w )]
    pixel_mask = [0 for x in range(0, h * w )]

    for i in range(pixel_mask_shape[0] ):
        if cls_data[i] >= cls_conf_threshold:
            pixel_mask[i] = 1
            points.append([i % w, i // w ])

            group_mask[i] = -1;
        else :
            pixel_mask[i] = 0

    link_data_size = link_data_shape[0] * link_data_shape[1] * link_data_shape[2] * link_data_shape[3]
    link_mask =  [0 for x in range(0, link_data_size)]
    for i in range(link_data_size):
        if link_data[i] >= link_conf_threshold:
            link_mask[i] = 1
        else:
            link_mask[i] = 0
    neighbours = (link_data_shape[3])

    for point in points:
        neighbour = 0

        for ny in range(point[1] - 2, point[1] + 3):
            for nx in range(point[0] - 2, point[0] + 3):
                if nx == point[0] and ny == point[1]:
                    continue
                if nx >=0 and nx < w and ny >= 0 and ny < h:
                    pixel_value = pixel_mask[ny * w + nx]
                    link_value = link_mask[(point[1] * w + point[0]) * neighbours + neighbour]
                    if pixel_value and link_value:
                        join(point[0] + point[1] * w, nx + ny * w, group_mask)
                neighbour = neighbour + 1

    return get_all(points, w, h, group_mask)

def maskToBoxes(mask, min_area, min_height, image_size):

    min_val, max_val, min_loc, max_loc = cv2.minMaxLoc(mask)
    resized_mask = mask

    boxes=[]

    for i in range(1, int(max_val) + 1):
        bbox_mask = np.zeros([resized_mask.shape[0], resized_mask.shape[1]], np.uint8)
        for w in range(resized_mask.shape[0]):
            for h in range(resized_mask.shape[1]):
                if int(resized_mask[w][h]) == i:
                    bbox_mask[w][h] = 255

        bbox_mask = cv2.resize(bbox_mask, (image_size[1], image_size[0]), cv2.INTER_NEAREST)

        contours, hierarchy = cv2.findContours(bbox_mask, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_SIMPLE)
        cv2.drawContours(bbox_mask, contours, -1, (0, 0, 255), 3)

        r = cv2.minAreaRect(contours[0])

        if r[1][0] < min_height or r[1][1] < min_height:
            continue
        if r[1][0] * r[1][1] < min_area:
            continue

        boxes.append(r)
    return boxes

def postProcess(out, image_size, cls_conf_threshold, link_conf_threshold):
    link_data = np.array(out[0])
    link_data = link_data.reshape(1, 16, 192, 320).transpose((0, 2, 3, 1))
    link_data = softmax(link_data)
    link_data = sliceAndGetSecondChannel(link_data)

    cls_data = np.array(out[1])
    cls_data = cls_data.reshape(1, 2, 192, 320).transpose((0, 2, 3, 1))
    cls_data = softmax(cls_data)
    cls_data = sliceAndGetSecondChannel(cls_data)

    mask = decodeImageByJoin(link_data, cls_data, (1, 192, 320, 8), (1, 192, 320, 1), link_conf_threshold, cls_conf_threshold)
    rects = maskToBoxes(mask,300,20,image_size)

    return rects

def topLeftPoint(points):
    most_left = [float("inf"), float("inf")]
    almost_most_left = [float("inf"), float("inf")]
    most_left_idx = -1
    almost_most_left_idx = -1

    for i in range(len(points)):
        if most_left[0] > points[i][0]:
            if most_left[0] < float("inf"):
                almost_most_left = most_left
                almost_most_left_idx = most_left_idx

            most_left = points[i]
            most_left_idx = i

        if almost_most_left[0] > points[i][0]:
            if points[i][0] != most_left[0] or points[i][1] != most_left[1]:
                almost_most_left = points[i]
                almost_most_left_idx = i


    if almost_most_left[1] < most_left[1]:
        most_left = almost_most_left
        most_left_idx = almost_most_left_idx
    return most_left_idx

def cropImage(image, points, target_size, top_left_point_idx):
    point0 = np.float32(points[top_left_point_idx])
    point1 = np.float32(points[(top_left_point_idx + 1) % 4])
    point2 = np.float32(points[(top_left_point_idx + 2) % 4])

    from_ = np.float32([point0, point1, point2])
    to_ = np.float32([[0, 0], [target_size[0] - 1, 0], [target_size[0] - 1, target_size[1] - 1]])
    M = cv2.getAffineTransform(from_, to_)

    res = cv2.warpAffine(image, M, (target_size[0], target_size[1]))

    return res


def softmax_(text_recognition_out, begin, end):
    max_element = np.argmax(text_recognition_out[begin:end])

    sum = 0
    for i in range(begin,end):
        sum += math.exp(text_recognition_out[i] - text_recognition_out[max_element])
    prob = 1 / sum
    return [max_element, prob]



def get_result(environ):

    start_time = time.time()

    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    model_xml="./models/text-detection-0003.xml"

    recognition_model_xml="./models/text-recognition-0012.xml"
    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    app_id = d.get(b'app_id')
    image_base64 = d.get(b'image')

    logger.info("AI-Service-Framework: ocr python service")
    if image_base64 == None:
        logger.info('AI-Service-Framework: cannot get image data')

    image_data = base64.b64decode(image_base64)
    image = cv2.imdecode(np.fromstring(image_data, np.uint8), cv2.IMREAD_COLOR)
    pic =list(image_data)
    pics = [pic]

    other_pin = rt_api.vectorVecFloat()

    link_data = rt_api.vectorFloat()
    cls_data = rt_api.vectorFloat()
    out = rt_api.vectorVecFloat()
    out.append(link_data)
    out.append(cls_data)

    urlinfo = rt_api.serverParams()
    urlinfo.url = 'https://api.ai.qq.com/fcgi-bin/ocr/ocr_generalocr'
    urlinfo.urlParam = data
    res = rt_api.infer_image(pics, 3, other_pin, model_xml, out, urlinfo)

    if res == 0:
        logger.info('AI-Service-Framework: local inference')
        cls_conf_threshold = 0.8
        link_conf_threshold = 0.8
        rects = postProcess(out, image.shape, cls_conf_threshold, link_conf_threshold)


        item_list = []
        for rect in rects:
            item_dict = {}
            item_dict["item"] = ""
            words_list = []

            points = cv2.boxPoints(rect)
            points = np.int0(points)
            top_left_point_idx = topLeftPoint(points)
            itemcoord_dict = {}
            itemcoord_dict["x"] = int(points[top_left_point_idx][0])
            itemcoord_dict["y"] = int(points[top_left_point_idx][1])

            itemcoord_dict["width"] = int(rect[1][0])
            itemcoord_dict["height"] = int(rect[1][1])

            item_dict["itemcoord"] = [itemcoord_dict]

            target_size = [120, 32]
            cropped_text = cropImage(image, points, target_size, top_left_point_idx)
            grey_cropped_text = cv2.cvtColor(cropped_text, cv2.COLOR_RGB2GRAY)
            success, encoded_image = cv2.imencode(".jpg", grey_cropped_text)
            #img_bytes = encoded_image.tostring()

            text_recognition_pic = list(encoded_image)
            text_recognition_pics = [text_recognition_pic]
            text_recognition_out = rt_api.vectorFloat()
            text_recognition_outs = rt_api.vectorVecFloat()
            text_recognition_outs.append(text_recognition_out)

            res = rt_api.infer_image(text_recognition_pics, 1, other_pin, recognition_model_xml, text_recognition_outs, urlinfo)

            alphabet = "0123456789abcdefghijklmnopqrstuvwxyz#"
            pad_symbol = '#'
            num_classes = len(alphabet)
            res = ""
            prev_pad = False

            for i in range(0, len(text_recognition_outs[0]), num_classes):
                result = softmax_(text_recognition_outs[0], i, i+num_classes)
                symbol = alphabet[result[0]]
                if symbol != pad_symbol:
                    if len(res) == 0 or prev_pad or (len(res) != 0 and symbol != res[len(res) - 1]):
                        prev_pad = False
                        res += symbol

                        word_dict = {}
                        word_dict["character"] = symbol
                        word_dict["confidence"] = result[1]
                        words_list.append(word_dict)
                else:
                    prev_pad = True
            item_dict["itemstring"] = res
            item_dict["word"] = words_list
            item_list.append(item_dict)

        ie_dict = {}

        ie_dict["item_list"] = item_list

        result_dict = {}
        result_dict["ret"] = 0
        result_dict["msg"] = "ok"
        result_dict["data"] = ie_dict

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
