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
import datetime
import numpy as np
import ctypes

import inferservice_python as rt_api
import socket
import grpc
import struct
import service_runtime_health_monitor_pb2
import service_runtime_health_monitor_pb2_grpc

from fcgi_utils import Hyperparams as hp
from fcgi_utils import spectrogram2wav, text_to_vector
from scipy.io.wavfile import write

import logging
import logging.handlers

syslog = logging.handlers.SysLogHandler(address='/dev/log')
msgfmt = '%(asctime)s {0} %(name)s[%(process)d]: %(message)s'.format(socket.gethostname())
formatter = logging.Formatter(msgfmt, datefmt='%b %d %H:%M:%S')
syslog.setFormatter(formatter)

logger = logging.getLogger(os.path.basename(sys.argv[0]))
logger.addHandler(syslog)
logger.setLevel(logging.DEBUG)

def get_default_gateway():
    with open("/proc/net/route") as proc_route:
        for line in proc_route:
            items = line.strip().split()
            if items[1] != '00000000' or not int(items[3], 16) & 2:
                continue
            return socket.inet_ntoa(struct.pack("<L", int(items[2], 16)))
    return None

def get_result(environ):

    start_time = time.time()

    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    model_xml="./models/tts-encoder-decoder.xml"
    model2_xml = "./models/tts-postprocessing.xml"

    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    pulse_server_status = 0
    if os.environ.get("PULSE_SERVER") == None:
        mointor_server_addr = get_default_gateway()
        if mointor_server_addr == None:
            pulse_server_status = 1
        else:
            try:
                os.environ["no_proxy"] = mointor_server_addr + ",localhost"
                with grpc.insecure_channel(mointor_server_addr + ':8082') as channel:
                    stub = service_runtime_health_monitor_pb2_grpc.MonitorStub(channel)
                    resp = stub.get_pulse_server_addr(service_runtime_health_monitor_pb2.Empty())
                    if resp.status != 0:
                        pulse_server_status = 1;
                    else:
                        os.environ["PULSE_SERVER"] = resp.addr
            except Exception as e:
                pulse_server_status = 1
    if pulse_server_status != 0:
        logger.info("AI-Service-Framework: the address of the pulseaudio service is error!")

    app_id = d.get(b'app_id')
    tts_data = d.get(b'text')

    logger.info("AI-Service-Framework: tts python service")
    if tts_data == None:
        logger.info('AI-Service-Framework: cannot get audio data')
    tts_data = [tts_data.decode()]
    texts = text_to_vector(tts_data)

    urlinfo = rt_api.serverParams()
    urlinfo.url = 'https://api.ai.qq.com/fcgi-bin/aai/aai_tts'
    urlinfo.urlParam = data

    for row in range(texts.shape[0]):
        y_hat = np.zeros((1, 200, hp.n_mels * hp.r), np.float32)

        input_data = rt_api.vectorVecFloat()
        text = texts[row].reshape(-1)
        x_pin = rt_api.vectorFloat(text)
        input_data.append(x_pin)
        input_vecs = rt_api.tripleVecFloat()
        input_vecs.append(input_data)

        other_pin = rt_api.vectorVecFloat()
        y_hat = y_hat.reshape(-1)
        y_pin = rt_api.vectorFloat(y_hat)
        other_pin.append(y_pin)

        out1 = rt_api.vectorFloat()
        out = rt_api.vectorVecFloat()
        out.append(out1)

        model2_additional_pin = rt_api.vectorVecFloat()
        out2 = rt_api.vectorFloat()
        model2_out = rt_api.vectorVecFloat()
        model2_out.append(out2)

        for i in range(48):
            res = rt_api.infer_common(input_vecs, other_pin, model_xml, "OPENVINO", out, urlinfo)
            for j in range(400):
                other_pin[0][i * 400 + j] = out[0][i * 400 + j]

            if res != 0:
                break
        if res != 0:
            break

        model2_input_vecs = rt_api.tripleVecFloat()
        model2_input_vecs.append(other_pin)
        res = rt_api.infer_common(model2_input_vecs, model2_additional_pin, model2_xml, "OPENVINO", model2_out, urlinfo)
        if res != 0:
            break

        mag = np.array(model2_out[0])
        mag = mag.reshape(1000, 1025)

        audio = spectrogram2wav(mag)
        if row == 0:
            audio_data = audio
        else:
            audio_data = np.append(audio_data, audio)


    if res == 0:
        logger.info('AI-Service-Framework: local inference')
        audio_path = (os.path.join("/tmp/", 's_compound.wav'))
        write(audio_path, hp.sr, audio_data)

        if pulse_server_status == 0:
            try:
                import soundcard
                default_speaker = soundcard.default_speaker()
                dummy_data = np.zeros(64000, dtype = np.float)
                with default_speaker.player(samplerate=hp.sr, blocksize=512) as sp:
                    sp.play(dummy_data)
                    sp.play(audio_data)
            except Exception as e:
                pulse_server_status = 1;

        f = open(audio_path, "rb")
        wav_data =f.read()
        base64_wave_data = base64.b64encode(wav_data)
        f.close()

        ie_dict = {}
        ie_dict["format"] = 2
        ie_dict["speech"] = str(base64_wave_data,encoding ="utf-8")
        f_md5sum = os.popen('md5sum ' + audio_path)
        md5sum = f_md5sum.read()
        ie_dict["md5sum"] = md5sum.split(" ")[0]
        f_md5sum.close()
        os.remove(audio_path)

        result_dict = {}
        result_dict["ret"] = 0
        if pulse_server_status != 0:
            result_dict["msg"] = "Only generates wave file, not send to the host speaker because of the pulseaudio service error!"
        else:
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
