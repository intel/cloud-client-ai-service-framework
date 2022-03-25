#!/usr/bin/python3
# Copyright (C) 2020 Intel Corporation

from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer
import subprocess
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

import socket
import grpc
import struct
import service_runtime_health_monitor_pb2
import service_runtime_health_monitor_pb2_grpc

import threading, queue
from threading import Lock,Thread,Event

import logging
import logging.handlers

import syslog as syslogger

TARGET_CHANNELS = [0] #[0, 1]
TARGET_SAMPLE_RATE = 16000
FRAMES_PER_BUFFER = int(80000) #(TARGET_SAMPLE_RATE / 10)
INT16_INFO = np.iinfo(np.int16)

syslog = logging.handlers.SysLogHandler(address=('localhost', 514), facility='user', socktype=socket.SOCK_DGRAM)
msgfmt = '%(asctime)s {0} %(name)s[%(process)d]: %(message)s'.format(socket.gethostname())
formatter = logging.Formatter(msgfmt, datefmt='%b %d %H:%M:%S')
syslog.setFormatter(formatter)

logger = logging.getLogger(os.path.basename(sys.argv[0]))
logger.addHandler(syslog)
logger.setLevel(logging.DEBUG)

L = []
stop = Event()
q = queue.Queue()
gAsrInit = Event()
gInitStatus = 0

def get_default_gateway():
    with open("/proc/net/route") as proc_route:
        for line in proc_route:
            items = line.strip().split()
            if items[1] != '00000000' or not int(items[3], 16) & 2:
                continue
            return socket.inet_ntoa(struct.pack("<L", int(items[2], 16)))
    return None

def scale_volume(np_data):
    volume = 1  # by default volume is at 100%
    np_data *= volume * INT16_INFO.max  # scale from float [-1, 1] to int16 range
    if volume != 1:
        np.clip(np_data, INT16_INFO.min, INT16_INFO.max, np_data)
    return np_data

def asr_thread():

    global gInitStatus

    try:
        import soundcard
    except Exception as err:
        gInitStatus = 1
        gAsrInit.set()
        exit(0)

    speech = np.zeros((10,), dtype = np.short)
    sampwidth = 2

    model_xml = './models/lspeech_s5_ext/FP32/speech_lib.cfg'
    buf = np.zeros((100 * 100), dtype = np.int8)
    utt_res = rt_api.vectorChar(buf)
    device = 'CPU'

    default_mic = soundcard.default_microphone()
    syslogger.syslog("def mic is: %s" %(default_mic.name))

    mode = 1
    res = rt_api.live_asr(mode, speech, int(sampwidth), model_xml, device, utt_res)
    gAsrInit.set()

    with default_mic.recorder(TARGET_SAMPLE_RATE, channels=TARGET_CHANNELS, blocksize=512) as mic:

        while not stop.is_set():

            mic_data = mic.record(FRAMES_PER_BUFFER)
            mic_data = scale_volume(mic_data)

            for j in range(100*100):
                utt_res[j] = '\u0000'

            mode = 2
            res = rt_api.live_asr(mode, mic_data, int(sampwidth), model_xml, device, utt_res)
            utt_str = ''.join(utt_res)
            q.put(utt_str.split('\u0000')[0])
            #syslogger.syslog("output utt is: %s" %(utt_str.split('\u0000')[0]))

    mode = 0
    for j in range(100*100):
        utt_res[j] = '\u0000'
    res = rt_api.live_asr(mode, speech, int(sampwidth), model_xml, device, utt_res)

def get_result(environ):

    start_time = time.time()

    try:
        request_body_size = int(environ.get('CONTENT_LENGTH', 0))
    except (ValueError):
        request_body_size = 0

    data = environ["wsgi.input"].read(request_body_size)
    d = dict((k, v[0]) for k, v in parse_qs(data).items())

    healthcheck = d.get(b'healthcheck')
    if healthcheck and int(healthcheck) == 1:
        healthcheck_str = "healthcheck ok"
        return healthcheck_str

    s_mode = d.get(b'mode')

    res = 0
    if int(s_mode) == 0:
        syslogger.syslog('AI-Service-Framework: start sthread')

        if os.environ.get("PULSE_SERVER") == None:
            mointor_server_addr = get_default_gateway()
            if mointor_server_addr == None:
                res = 2
            else:
                try:
                    os.environ["no_proxy"] = mointor_server_addr + ",localhost"
                    with grpc.insecure_channel(mointor_server_addr + ':8082') as channel:
                        stub = service_runtime_health_monitor_pb2_grpc.MonitorStub(channel)
                        resp = stub.get_pulse_server_addr(service_runtime_health_monitor_pb2.Empty())
                        if resp.status != 0:
                            res = 2;
                        else:
                            os.environ["PULSE_SERVER"] = resp.addr
                except Exception as e:
                    res = 2
        if (res == 0):
            global gInitStatus
            gInitStatus = 0
            gAsrInit.clear()
            stop.clear()
            p = Thread(target=asr_thread, daemon=True)
            p.start()
            gAsrInit.wait()
            if gInitStatus != 0:
                p.join()
                res = 2
                utt = 'Starting live asr failure!'
            else:
                L.append(p)
                utt = 'Starting live asr ok!'
    if int(s_mode) == 1:
        utt = q.get()
        syslogger.syslog("fcgi get is: %s" %(utt))
    if int(s_mode) == 2:
        stop.set()
        if len(L) > 0:
            p = L[0]
            p.join()
            L.remove(p)
        utt = 'Stop live asr ok!'

    if res == 0:
        logger.info('AI-Service-Framework: local inference')
        ie_dict = {}
        ie_dict["text"] = utt
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
        syslogger.syslog('AI-Service-Framework: inference error')
        result_dict = {}
        result_dict["ret"] = 1
        if res == 2:
            result_dict["msg"] = "The pulse server ip is error, try again after os fully bootup!"
        else:
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

