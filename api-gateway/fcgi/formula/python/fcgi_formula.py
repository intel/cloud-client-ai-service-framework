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

import json
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


def preprocess_image( image_raw, tgt_shape):
    img_h, img_w = image_raw.shape[0:2]
    target_height, target_width = tgt_shape
    new_h = min(target_height, img_h)
    new_w = min(target_width, img_w)

    image_raw=image_raw[:new_h, :new_w, :]
    image = cv2.copyMakeBorder(image_raw, 0, target_height - img_h,
                               0, target_width - img_w, cv2.BORDER_CONSTANT,
                               None, (255,255,255))
    return image

class formula():#convert index to formula

    def __init__(self, vocab_file):
        assert vocab_file.endswith(".json"), "vocab file must be json file"

        with open(vocab_file, "r") as file:
            dict_vocab = json.load(file)
            dict_vocab['id2sign'] = {int(i): j for i, j in dict_vocab['id2sign'].items()}

            self.index = dict_vocab["id2sign"]

    def get_formula(self, targets):#get latex formula from index
        phrase_formula = []

        for target in targets:
            if target == 2:
                break
            phrase_formula.append(
                self.index.get(target, "?"))
        return " ".join(phrase_formula)


def get_characters(charlist):
    '''Get characters'''
    with open(charlist, 'r', encoding='utf-8') as f:
        return ''.join(line.strip('\n') for line in f)

def get_result(environ):
    start_time = time.time()

    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    model_xml="./models/formula-recognition-medium-scan-0001-im2latex-encoder.xml"
    vocab_file = "./models/vocab.json"
    vocab = formula(vocab_file)

    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    app_id = d.get(b'app_id')
    image_base64 = d.get(b'image')

    logger.info("AI-Service-Framework: formula python service")
    if image_base64 == None:
        logger.info('AI-Service-Framework: cannot get image data')
    image_data = base64.b64decode(image_base64)
    ori_img_ = np.fromstring(image_data, np.uint8)
    ori_img = cv2.imdecode(ori_img_, cv2.IMREAD_COLOR)
    ori_img = preprocess_image(ori_img, (160,1400))

    img_encode = cv2.imencode('.jpg',ori_img)[1]
    pic = list(img_encode)
    pics = [pic]

    other_pin = rt_api.vectorVecFloat()

    #prepare out
    dec_states_h = rt_api.vectorFloat()
    dec_states_c = rt_api.vectorFloat()
    output = rt_api.vectorFloat()
    row_enc_out = rt_api.vectorFloat()
    out = rt_api.vectorVecFloat()
    out.append(dec_states_h)
    out.append(dec_states_c)
    out.append(output)
    out.append(row_enc_out)

    urlinfo = rt_api.serverParams()
    urlinfo.url = 'https://api.ai.qq.com/fcgi-bin/image/image_tag'
    urlinfo.urlParam = data
    res = rt_api.infer_image(pics, 3, other_pin, model_xml, out, urlinfo)
    logits =[]

    if res == 0:
        logger.info('AI-Service-Framework: local inference')

        max_formula_len = 128
        dec_states_h = out[0]
        dec_states_c = out[1]

        output = out[2]
        row_enc_out = out[3]
        tgt = [[[0]]]

        for _ in range(max_formula_len):
            decode_model = "./models/formula-recognition-medium-scan-0001-im2latex-decoder.xml"
            other_pin = rt_api.vectorVecFloat()
            other_pin.append(rt_api.vectorFloat(dec_states_c))
            other_pin.append(rt_api.vectorFloat(output))
            other_pin.append(rt_api.vectorFloat(row_enc_out))
            other_pin.append(rt_api.vectorFloat([tgt[0][0][0]]))

            decode_out= rt_api.vectorVecFloat()
            decode_out1 = rt_api.vectorFloat()
            decode_out2 = rt_api.vectorFloat()
            decode_out3 = rt_api.vectorFloat()
            decode_out4 = rt_api.vectorFloat()
            decode_out.append(decode_out1)
            decode_out.append(decode_out2)
            decode_out.append(decode_out3)
            decode_out.append(decode_out4)

            input_data = rt_api.vectorVecFloat()
            x_pin1 = rt_api.vectorFloat(dec_states_h)
            input_data.append(x_pin1)
            input_vecs = rt_api.tripleVecFloat()
            input_vecs.append(input_data)

            res = rt_api.infer_common(input_vecs, other_pin, decode_model, "OPENVINO", decode_out, urlinfo)

            dec_states_h = decode_out[0]
            dec_states_c = decode_out[1]
            output = decode_out[3]
            logit = np.array(decode_out[2]).reshape(1,101)
            logits.append(logit)
            tgt = np.array([[np.argmax(logit, axis=1)]])
            tgt = tgt.tolist()

            if tgt[0][0][0] == 2:
                break

        logits = np.array(logits)
        logits = logits.squeeze(axis=1)
        targets = np.argmax(logits, axis=1)
        formula_result = vocab.get_formula(targets)
        result_dict={}
        result_dict["ret"] = 0
        result_dict["msg"] = "ok"
        result_dict["data"] = formula_result

        server_running_time = round(time.time() - start_time, 3)
        result_dict["time"] = server_running_time
        result_json = json.dumps(result_dict, indent = 1)

    elif res == 1:
        logger.info('AI-Service-Framework: remote inference')
        result_dict = {}
        result_dict["ret"] = 2
        result_dict["msg"] = "no remote inference"
        result_json = json.dumps(result_dict, indent = 1)

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
