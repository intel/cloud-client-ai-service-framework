#!/usr/bin/python3
# Copyright (C) 2020 Intel Corporation

from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer
import threading
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
from fcgi_codec import CTCCodec
import cv2
from shapely.geometry import Polygon
import pyclipper
import math
import copy
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

class formula():

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

    def latex_preprocess_image( image_raw, tgt_shape):
        img_h, img_w = image_raw.shape[0:2]
        target_height, target_width = tgt_shape
        new_h = min(target_height, img_h)
        new_w = min(target_width, img_w)

        image_raw=image_raw[:new_h, :new_w, :]
        image = cv2.copyMakeBorder(image_raw, 0, target_height - img_h,
                                   0, target_width - img_w, cv2.BORDER_CONSTANT,
                                   None, (255,255,255))
        return image

    def latex_recognizer(latex_crop_list, latex_encode_xml, latex_decode_xml, urlinfo,vocab, formula_result_list):

        for img in latex_crop_list:
            img_decode = formula.latex_preprocess_image(img, (160, 1400))
            img_encode = cv2.imencode('.jpg', img_decode)[1]

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
            res = rt_api.infer_image(pics, 3, other_pin, latex_encode_xml, out, urlinfo)
            logits = []

            if res == 0:
                max_formula_len = 128
                dec_states_h = out[0]
                dec_states_c = out[1]
                output = out[2]
                row_enc_out = out[3]
                tgt = [[[0]]]

                for _ in range(max_formula_len):
                    decode_model = latex_decode_xml
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
                formula_result_list.append(formula_result)


class chinese_handwritten():

    def get_characters(charlist):
        '''Get characters'''
        with open(charlist, 'r', encoding='utf-8') as f:
            return ''.join(line.strip('\n') for line in f)

    def handwritten_image_preprocess(image, height, width):
        image_ratio = float(image.shape[1]) / float(image.shape[0])
        rw = int(height * image_ratio)

        if rw <= 2000:
            resized_image = cv2.resize(image, (rw, height), interpolation=cv2.INTER_AREA).astype(np.float32)
            resized_img = resized_image[None, :, :]

            _, rh, rw = resized_img.shape
            pad_resized_img = np.pad(resized_img, ((0, 0), (0, height - rh), (0, width -  rw)), mode='edge')
        else:
            image_ratio = width / image.shape[1]
            rh = int(image.shape[0] * image_ratio)
            resized_img = cv2.resize(image, (width, rh) , interpolation=cv2.INTER_AREA).astype(np.float32)
            resized_img = resized_img[None, :, :]
            _, rh, rw = resized_img.shape
            pad_resized_img = np.pad(resized_img, ((0, 0), (0, height - rh), (0, width -  rw)), mode='edge')

        return pad_resized_img

    def handwritten_recognizer(handwritten_crop_list, model_xml, model_label, urlinfo, handwritten_result_list):

        for img in handwritten_crop_list:
            img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            img = chinese_handwritten.handwritten_image_preprocess(img, 96, 2000)
            img = img[0]
            image_data = cv2.imencode('.jpg',img)[1]
            pic = list(image_data)
            pics = [pic]

            other_pin = rt_api.vectorVecFloat()
            out1 = rt_api.vectorFloat()
            out = rt_api.vectorVecFloat() ##with opaque and stl_binding
            out.append(out1)

            res = rt_api.infer_image(pics, 3, other_pin, model_xml, out, urlinfo)
            if res == 0:
                char_label = chinese_handwritten.get_characters(model_label)
                code_ocr = CTCCodec(char_label, 20)
                predict = np.array(out[0])
                predict = predict.reshape(186,1,4059)
                result = code_ocr.decode(predict)
                handwritten_result_list.append(result[0])

class ppocr():

    def small_rectangle(contour_img):
        rectangle = cv2.minAreaRect(contour_img)
        left_top, right_top, right_down, left_down = 0, 1, 2, 3
        box_points = sorted(list(cv2.boxPoints(rectangle)), key=lambda x: x[0])

        if box_points[3][1] > box_points[2][1]:
            right_top = 2
            right_down = 3
        else:
            right_top = 3
            right_down = 2
        if box_points[1][1] > box_points[0][1]:
            left_top = 0
            left_down = 1
        else:
            left_top = 1
            left_down = 0

        rectangle_points = [box_points[left_top], box_points[right_top], box_points[right_down], box_points[left_down]]
        return rectangle_points, min(rectangle[1])


    def rectangle_score(bit_img, _rectangle):
        rectangle = _rectangle.copy()
        h, w = bit_img.shape[:2]

        w_min = np.clip(np.floor(rectangle[:, 0].min()).astype(np.int), 0, w - 1)
        h_min = np.clip(np.floor(rectangle[:, 1].min()).astype(np.int), 0, h - 1)
        w_max = np.clip(np.ceil(rectangle[:, 0].max()).astype(np.int), 0, w - 1)
        h_max = np.clip(np.ceil(rectangle[:, 1].max()).astype(np.int), 0, h - 1)


        rectangle[:, 0] = rectangle[:, 0] - w_min
        rectangle[:, 1] = rectangle[:, 1] - h_min

        mask_img = np.zeros((h_max - h_min + 1, w_max - w_min + 1), dtype=np.uint8)
        cv2.fillPoly(mask_img, rectangle.reshape(1, -1, 2).astype(np.int32), 1)
        return cv2.mean(bit_img[h_min:h_max + 1, w_min:w_max + 1], mask_img)[0]



    def large_rectangle(rectangle):
        enlarge_ratio = 1
        pco = pyclipper.PyclipperOffset()

        poly = Polygon(rectangle)
        length = poly.length
        area = poly.area
        ratio = area * enlarge_ratio / length

        pco.AddPath(rectangle, pyclipper.JT_ROUND, pyclipper.ET_CLOSEDPOLYGON)
        expanded_box = np.array(pco.Execute(ratio))
        return expanded_box


    def bit_img_boxes(predict, _bit_img, ori_width, ori_height):
        max_candidates = 1000
        bit_img = _bit_img
        height, width = bit_img.shape

        contours = cv2.findContours((bit_img * 255).astype(np.uint8), cv2.RETR_LIST,cv2.CHAIN_APPROX_SIMPLE)
        contours_num = len(contours)
        if contours_num == 2:
            new_contours, _ = contours[0], contours[1]
        elif contours_num == 3:
            img, new_contours, _ = contours[0], contours[1], contours[2]
        contours_num = min(contours_num, max_candidates)

        boxes =[]
        box_scores =[]


        for contour in new_contours:
            box, s_height = ppocr.small_rectangle(contour)
            min_size = 3
            if s_height < min_size:
                continue

            box = np.array(box)
            box_score = ppocr.rectangle_score(predict, box.reshape(-1, 2))
            if box_score < 0.5:
                continue

            large_box = ppocr.large_rectangle(box).reshape(-1, 1, 2)
            large_box, s_height = ppocr.small_rectangle(large_box)
            if s_height < min_size+2:
                continue

            large_box = np.array(large_box)

            w = np.round(large_box[:, 0] / width * ori_width)
            h = np.round(large_box[:, 1] / height * ori_height)
            large_box[:, 0] = np.clip(w, 0, ori_width)
            large_box[:, 1] = np.clip(h, 0, ori_height)

            boxes.append(large_box.astype(np.int16))
            box_scores.append(box_score)

        boxes = np.array(boxes, dtype=np.int16)
        return  boxes, box_scores

    def get_text_img(ori_img, box):
        top_width = np.linalg.norm(box[0] - box[1])
        down_width = np.linalg.norm(box[2] - box[3])
        crop_img_width = int(max(top_width, down_width))

        left_height = np.linalg.norm(box[0] - box[3])
        right_height = np.linalg.norm(box[1] - box[2])
        crop_img_height = int(max(left_height,right_height))

        points_std = np.float32([[0, 0], [crop_img_width, 0],
                                 [crop_img_width, crop_img_height],
                                 [0, crop_img_height]])

        box = box.astype(np.float32)
        M = cv2.getPerspectiveTransform(box, points_std)
        text_img = cv2.warpPerspective(
            ori_img,
            M, (crop_img_width, crop_img_height),
            borderMode=cv2.BORDER_REPLICATE,
            flags=cv2.INTER_CUBIC)
        text_img_height, text_img_width = text_img.shape[0:2]
        if text_img_height * 1.0 / text_img_width >= 1.5:
            text_img = np.rot90(text_img)
        return text_img

    def boxes_sorted(boxes):
        number = boxes.shape[0]
        dt_boxes = sorted(boxes, key=lambda x: (x[0][1], x[0][0]))
        dt_boxes = list(dt_boxes)

        for i in range(number - 1):
            if abs(dt_boxes[i + 1][0][1] - dt_boxes[i][0][1]) < 10 and \
               (dt_boxes[i][0][0] > dt_boxes[i + 1][0][0]):
                tmp_box = dt_boxes[i]
                dt_boxes[i] = dt_boxes[i + 1]
                dt_boxes[i + 1] = tmp_box
        return dt_boxes

    def det_postprocess(out, ori_img):
        thresh = 0.5
        boxes_batch = []

        seg_img = np.array(out[0])
        seg_img = seg_img.reshape(640,640,1)
        predict = seg_img.reshape([1, seg_img.shape[0], seg_img.shape[1]])
        seg_matrix = predict > thresh
        kernel = np.array([[1, 1], [1, 1]])

        for index in range(predict.shape[0]):
            src = np.array(seg_matrix[index]).astype(np.uint8)
            mask_img = cv2.dilate(src, kernel)
            boxes, box_score = ppocr.bit_img_boxes(predict[index], mask_img, ori_img.shape[1], ori_img.shape[0])
            sorted_boxes = ppocr.boxes_sorted(boxes)

            crop_img_list = []
            for index in range(len(sorted_boxes)):
                tmp_box = copy.deepcopy(sorted_boxes[index])
                crop_img = ppocr.get_text_img(ori_img, tmp_box)
                crop_img_list.append(crop_img)

        return crop_img_list, sorted_boxes


    def text_detection(pics, model_xml, urlinfo):

        other_pin = rt_api.vectorVecFloat()
        out1 = rt_api.vectorFloat()
        out = rt_api.vectorVecFloat()
        out.append(out1)

        res = rt_api.infer_image(pics, 3, other_pin, model_xml, out, urlinfo)
        return res, out


    def resize_rec_img(img, max_wh_ratio):

        length = round(img.shape[1] / 100) * 100
        if length == 0:
            length = 100
        if length > 1200:
            length = 1200
        dest_channel, dest_height, dest_width = [3, 32, length]

        resized_image = cv2.resize(img, (dest_width, dest_height))
        resized_image = resized_image.transpose((2, 0, 1))
        pad_img = np.zeros((dest_channel, dest_height, dest_width), dtype=np.float32)
        pad_img[:, :, 0:dest_width] = resized_image
        return pad_img


    def text_decode(preds_index, preds_prob=None, delete_duplicates=False):
        label_path = "./models/ppocr/ppocr_keys_v1.txt"
        use_space_char = True
        number = len(preds_index)
        label_str = ""

        with open(label_path, "rb") as f:
            lines = f.readlines()
            for line in lines:
                line = line.decode('utf-8').strip("\n")
                line = line.strip("\r\n")
                label_str += line

        if use_space_char:
            label_str += " "

        dict_label = list(label_str)
        dict_label = ['blank'] + dict_label
        ignored_characters = [0]#for ctc blank
        results = []

        for index in range(number):
            characters = []
            confidences = []
            for idx in range(len(preds_index[index])):

                if delete_duplicates and  idx > 0 and preds_index[index][idx - 1] == preds_index[index][idx]:
                    continue
                if preds_index[index][idx] in ignored_characters:
                    continue

                characters.append(dict_label[int(preds_index[index][idx])])
                if preds_prob is not None:
                    confidences.append(preds_prob[index][idx])
                else:
                    confidences.append(1)

            result = ''.join(characters)
            results.append((result, np.mean(confidences)))
        return results



    def text_recognizer(crop_img_list, rec_xml, urlinfo, ppocr_result):

        number = len(crop_img_list)
        ratio_list = []
        for img in crop_img_list:
            ratio_list.append(img.shape[1] / float(img.shape[0]))
            # Sorting can speed up the recognition process
        sorted_idx = np.argsort(np.array(ratio_list))

        results = [['', 0.0]] * number
        batch_size = 1

        for begin_num in range(0, number, batch_size):
            end_num = min(number, begin_num + batch_size)
            pics = []
            max_ratio = 0

            for ino in range(begin_num, end_num):
                height, width = crop_img_list[sorted_idx[ino]].shape[0:2]
                ratio = width * 1.0 / height
                max_ratio = max(max_ratio, ratio)

            for ino in range(begin_num, end_num):
                resized_img = ppocr.resize_rec_img(crop_img_list[sorted_idx[ino]],
                                                   max_ratio)

                length = round(crop_img_list[sorted_idx[ino]].shape[1] / 100) * 100
                if length == 0:
                    length = 100
                if length >1200:
                    length = 1200

                img_decode = resized_img.transpose((1,2,0))
                img_encode = cv2.imencode('.jpg',img_decode)[1]
                pic = list(img_encode)
                pics.append(pic)

            other_pin = rt_api.vectorVecFloat()
            out1 = rt_api.vectorFloat()
            out = rt_api.vectorVecFloat() ##with opaque and stl_binding
            out.append(out1)


            new_rec_xml = rec_xml.split(".xml")[0] + "_" + str(length) + ".xml"
            res = rt_api.infer_image(pics, 3, other_pin, new_rec_xml, out, urlinfo)
            preds = np.array(out[0])
            preds = preds.reshape(1, int(25 * length / 100), 6625)
            preds_idx = preds.argmax(axis = 2)
            preds_prob = preds.max(axis = 2)

            result = ppocr.text_decode(preds_idx, preds_prob, delete_duplicates=True)
            for index in range(len(result)):
                results[sorted_idx[begin_num + index]] = result[index]
        for result in results:
            ppocr_result.append(result[0])



def in_box(latex, dt_boxes):
    if latex[1] > dt_boxes[0][1] and latex[1] < dt_boxes[2][1] and \
       latex[0] > dt_boxes[0][0] and latex[0] < dt_boxes[2][0]:
        return True
    else:
        return False

def choose_crop_img( img_crop_list, point_crop_list, dt_boxes, latex_boxes, all_result, result_name, urlinfo):

    pics = []
    for i in range(len(img_crop_list)):
        img = img_crop_list[i].copy()
        height = img.shape[0]
        width = img.shape[1]
        if width > height *5:
            img = img[:,:height*5,:]
        else:
            black = [0,0,0]
            imge = cv2.copyMakeBorder(img, 0, 0, 0, height*5-width, cv2.BORDER_CONSTANT, value=black)

        img = cv2.resize(img,(80,400))
        img_encode = cv2.imencode('.jpg', img)[1]

        pic = list(img_encode)
        pics.append(pic)

    model_xml = "./models/formula_word.xml"


    out1 = rt_api.vectorFloat()
    out = rt_api.vectorVecFloat() ##with opaque and stl_binding
    out.append(out1)
    other_pin = rt_api.vectorVecFloat()

    res = rt_api.infer_image(pics, 1, other_pin, model_xml, out, urlinfo)

    latex_num = 0
    for i in range(len(img_crop_list)):
        if out[0][i] <0.5:
            latex_boxes.append(dt_boxes[i])
            point_crop_list.append(img_crop_list[i])
            all_result[i] = str(result_name) + ' '+str(latex_num)
            latex_num += 1



def get_result(environ):

    start_time = time.time()

    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    det_xml = "./models/ppocr/det.xml"
    cls_xml = "./models/ppocr/cls.xml"
    rec_xml = "./models/ppocr/rec.xml"

    latex_encode_xml = "./models/formula-recognition-medium-scan-0001-im2latex-encoder.xml"
    latex_decode_xml = "./models/formula-recognition-medium-scan-0001-im2latex-decoder.xml"
    vocab_file = "./models/vocab.json"
    vocab = formula(vocab_file)

    handwritten_xml = "./models/handwritten-simplified-chinese-recognition-0001.xml"
    handwritten_label = './models/scut_ept_char_list.txt'
    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    app_id = d.get(b'app_id')
    image_base64 = d.get(b'image')

    if image_base64 == None:
        logger.info('AI-Service-Framework: cannot get image data')
    image_data = base64.b64decode(image_base64)
    pic = list(image_data)
    pics = [pic]

    ori_img_ = np.fromstring(image_data, np.uint8)
    ori_img = cv2.imdecode(ori_img_, cv2.IMREAD_COLOR)
    ori_img_width, ori_img_height, _ = ori_img.shape


    logger.info("AI-Service-Framework: digitalnote python service")

    urlinfo = rt_api.serverParams()
    urlinfo.url = 'https://api.ai.qq.com/fcgi-bin/image/image_tag'
    urlinfo.urlParam = data

    res, out = ppocr.text_detection(pics, det_xml, urlinfo)

    if res == 0:
        logger.info('AI-Service-Framework: local inference')

        img_crop_list, dt_boxes = ppocr.det_postprocess(out, ori_img)
        latex_crop_list = []
        latex_boxes = []
        all_result = [0]*len(dt_boxes)

        choose_crop_img( img_crop_list, latex_crop_list, dt_boxes, latex_boxes, all_result, "latex_result",urlinfo )



        ppocr_crop_list = []
        ppocr_boxes = []
        ppocr_num = 0
        for i in range(len(all_result)):
            if all_result[i] == 0:
                ppocr_crop_list.append(img_crop_list[i])
                ppocr_boxes.append(dt_boxes[i])
                all_result[i] = "ppocr_result " + str(ppocr_num)
                ppocr_num += 1

        latex_result = []
        ppocr_result = []
        threads = []

        t1 = threading.Thread(target=formula.latex_recognizer, args=(latex_crop_list,latex_encode_xml,latex_decode_xml,urlinfo,vocab,latex_result))
        threads.append(t1)

        t3 = threading.Thread(target=ppocr.text_recognizer, args=(ppocr_crop_list, rec_xml, urlinfo, ppocr_result))
        threads.append(t3)

        for t in threads:
            t.setDaemon(True)
            t.start()
        for t in threads:
            t.join()

        result_dict = { 'latex_result':latex_result, 'ppocr_result':ppocr_result}
        result = ''

        for i in all_result:
            if i == 0:
                continue
            list_name, index = i.split(' ')
            result += result_dict[list_name][int(index)]+'\n '
        result_dict = {}
        result_dict["ret"] = 0
        result_dict["msg"] = "ok"
        result_dict["data"] = result
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
