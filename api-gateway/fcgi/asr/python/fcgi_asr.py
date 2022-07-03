#!/usr/bin/python3
# Copyright (C) 2020 - 2022 Intel Corporation

from html import escape
from urllib.parse import parse_qs
from flup.server.fcgi import WSGIServer

import json
import base64
import os
import sys
import datetime
import ctypes
import time
import wave
import grpc
import struct
import numpy as np

from asr.asr_utils.profiles import PROFILES
from asr.asr_utils.deep_speech_seq_pipeline import DeepSpeechSeqPipeline
import webrtcvad

import threading, queue, collections
from threading import Lock,Thread,Event

import service_runtime_health_monitor_pb2
import service_runtime_health_monitor_pb2_grpc

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

TARGET_CHANNELS = [0] #[0, 1]
TARGET_SAMPLE_RATE = 16000
FRAMES_PER_BUFFER = int(320) #int(1600) #(TARGET_SAMPLE_RATE / 10) 每个frame是16 bits,共1600个frame,即1600个样本,3200个 bytes
INT16_INFO = np.iinfo(np.int16)
BLOCKS_PER_SECOND = 50

args_device = 'CPU'
args_beam_width = 500
args_max_candidates = 1
args_realtime_window = 79
channel_num = 1

gThreadPool = []
gAudioBuffer = queue.Queue()
gResultBuffer = queue.Queue()
gAsrStop = Event()
gOnlineThreadRunning = False

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

def asr_mic_thread(using_mic, simulation_data):

    if using_mic:
        try:
            import soundcard
        except Exception as err:
            logger.info("import soundcard err!")
            exit(0)

        default_mic = soundcard.default_microphone()
        logger.info(default_mic.name)

        with default_mic.recorder(TARGET_SAMPLE_RATE, channels=TARGET_CHANNELS) as mic:
            while not gAsrStop.is_set():
                mic_data = mic.record(FRAMES_PER_BUFFER)
                mic_data = scale_volume(mic_data)
                frames = np.ndarray.tostring(mic_data.astype(np.int16))
                gAudioBuffer.put(frames)
    else:
        if simulation_data == None:
            exit(0)

        frame_num = int(len(simulation_data) / 2)
        i = 0
        while not gAsrStop.is_set():
            # simulate mic input with a wave file
            if i + 320 < frame_num :
                wf_frames = simulation_data[i * 2 : (i + 320) * 2]
                i = i + 320
            elif i == frame_num:
                wf_frames = bytes(320 * 2)
            else:
                wf_frames = simulation_data[i * 2 : ]
                padding = bytes((320 - (frame_num - i)) *2)
                wf_frames = wf_frames + padding
                i = frame_num

            gAudioBuffer.put(wf_frames)
            time.sleep(0.02)

def vad_collector(sample_rate, frame_duration_ms,
                  padding_duration_ms, vad, ratio=0.75, frames=None):
    num_padding_frames = padding_duration_ms // frame_duration_ms
    ring_buffer = collections.deque(maxlen=num_padding_frames)
    triggered = False

    while True:
        try:
            frame = gAudioBuffer.get(block = True, timeout = 5)
        except queue.Empty:
            frame = None

        if gAsrStop.is_set():
            break

        if frame is None:
            continue

        if len(frame) < 640:
            return

        is_speech = vad.is_speech(frame, sample_rate)

        if not triggered:
            ring_buffer.append((frame, is_speech))
            num_voiced = len([f for f, speech in ring_buffer if speech])
            if num_voiced > ratio * ring_buffer.maxlen:
                triggered = True
                for f, s in ring_buffer:
                    yield f
                ring_buffer.clear()

        else:
            yield frame
            ring_buffer.append((frame, is_speech))
            num_unvoiced = len([f for f, speech in ring_buffer if not speech])
            if num_unvoiced > ratio * ring_buffer.maxlen:
                triggered = False
                yield None
                ring_buffer.clear()

def get_profile(profile_name):
    if profile_name in PROFILES:
        return PROFILES[profile_name]

def create_stt(language, realtime):

    model_xml = '/opt/fcgi/cgi-bin/models/mozilla-deepspeech-0.8.2.xml'
    lm_file = '/opt/fcgi/cgi-bin/models/deepspeech-0.8.2-models.kenlm'
    args_profile = 'mds08x_en'
    if language == 'CHINESE':
        args_profile = 'mds09x_ch'
        model_xml = '/opt/fcgi/cgi-bin/models/deepspeech-0.9.3-models-zh-CN.xml'
        lm_file = '/opt/fcgi/cgi-bin/models/deepspeech-0.9.3-models-zh-CN.kenlm'

    profile = get_profile(args_profile)

    stt = DeepSpeechSeqPipeline(
        language = language,
        model = model_xml,
        lm = lm_file,
        beam_width = args_beam_width,
        max_candidates = args_max_candidates,
        profile = profile,
        device = args_device,
        online_decoding = realtime,
    )

    return profile, stt

def asr_ctc_thread(language):

    profile, stt = create_stt(language, True)

    vad = webrtcvad.Vad(int(3))
    block_size = int(TARGET_SAMPLE_RATE / BLOCKS_PER_SECOND)
    frame_duration_ms = 1000 * block_size // TARGET_SAMPLE_RATE #20ms per block, total 50
    frames = vad_collector(TARGET_SAMPLE_RATE, frame_duration_ms, 300, vad)
    audio_pos = 0
    for frame in frames:

        if gAsrStop.is_set():
            return

        if frame is not None:
            audio_block = np.frombuffer(frame, dtype=np.int16).reshape((-1, 1))

            if audio_block.shape[0] == 0:
                break
            audio_pos += audio_block.shape[0]

            partial_transcr = stt.recognize_audio(audio_block, TARGET_SAMPLE_RATE, finish=False)
        else:
            transcription = stt.recognize_audio(None, TARGET_SAMPLE_RATE, finish=True)
            gResultBuffer.put(transcription[0].text)

def return_message(res, message, start_time):

    ie_dict = {}
    ie_dict["text"] = message
    result_dict = {}
    if res == 0:
        result_dict["ret"] = 0
        result_dict["msg"] = "ok"
    else:
        result_dict["ret"] = 1
        result_dict["msg"] = "error"

    result_dict["data"] = ie_dict
    server_running_time = round(time.time() - start_time, 3)
    result_dict["time"] = server_running_time
    result_json = json.dumps(result_dict, indent = 1)

    return result_json

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

    realtime = d.get(b'realtime').decode()
    if realtime == 'OFFLINE':
        args_realtime = False
    elif realtime == 'ONLINE_READ' or realtime == 'ONLINE_STOP':
        args_realtime = True
    else:
        msg = 'The realtime parameter error!'
        return return_message(1, msg, start_time)

    language = d.get(b'language').decode()

    global gOnlineThreadRunning

    if not args_realtime:
        if gOnlineThreadRunning:
            # close thread
            gAsrStop.set()
            for p in gThreadPool:
                p.join()
            gThreadPool.clear()
            gOnlineThreadRunning = False

        sampling_rate = int(d.get(b'rate'))
        audio_base64 = d.get(b'speech')
        if audio_base64 == None:
            msg = 'Error: no speech data!'
            return return_message(1, msg, start_time)

        audio_data = base64.b64decode(audio_base64)
        speech = np.frombuffer(audio_data, dtype = np.short)
        sample_width = int(d.get(b'samplewidth'))
        pcm_length = int(len(audio_data) / 2)

        profile, stt = create_stt(language, args_realtime)

        sr = profile['model_sampling_rate']
        args_block_size = round(sr * 10) if not args_realtime else round(sr * profile['frame_stride_seconds'] * 16) #512*16 (frame)

        assert sample_width == 2, "Only 16-bit WAV PCM supported"
        assert channel_num == 1, "Only mono WAV PCM supported"
        assert abs(sampling_rate / profile['model_sampling_rate'] - 1) < 0.1, "Only {} kHz WAV PCM supported".format(profile['model_sampling_rate'] / 1e3)

        audio_pos = 0
        for audio_iter in range(0, pcm_length, args_block_size):
            audio_block = speech[audio_pos : audio_pos + args_block_size]
            audio_block = audio_block.reshape((-1, channel_num))

            if audio_block.shape[0] == 0:
                break
            audio_pos += audio_block.shape[0]

            partial_transcr = stt.recognize_audio(audio_block, sampling_rate, finish=False)

        transcription = stt.recognize_audio(None, sampling_rate, finish=True)

        text_str = ""
        if transcription is not None and len(transcription) > 0:
            for candidate in transcription:
                text_str += candidate.text
        else:
            text_str = 'offline asr error'

        return return_message(0, text_str, start_time)

    else:
        if realtime == 'ONLINE_READ':
            if not gOnlineThreadRunning:
                use_mic = d.get(b'audio_input').decode()
                if use_mic == 'MIC':
                    isUsingMic = True
                    audio_data = None
                else:
                    isUsingMic = False
                    audio_base64 = d.get(b'speech')
                    if audio_base64 == None:
                        msg = 'Error: no speech data!'
                        return return_message(1, msg, start_time)
                    audio_data = base64.b64decode(audio_base64)

                if isUsingMic and os.environ.get("PULSE_SERVER") == None:
                    mointor_server_addr = get_default_gateway()
                    res = 1
                    if mointor_server_addr != None:
                        try:
                            os.environ["no_proxy"] = mointor_server_addr + ",localhost"
                            with grpc.insecure_channel(mointor_server_addr + ':8082') as channel:
                                stub = service_runtime_health_monitor_pb2_grpc.MonitorStub(channel)
                                resp = stub.get_pulse_server_addr(service_runtime_health_monitor_pb2.Empty())
                                if resp.status == 0:
                                    os.environ["PULSE_SERVER"] = resp.addr
                                    logger.info(resp.addr)
                                    res = 0;
                        except Exception as e:
                           res = 2
                    if res != 0:
                        logger.info('Pulse server set error')
                        msg = 'Error to get pulse server ip address. Please run pulseaudio and health-monitor services!'
                        return return_message(1, msg, start_time)

                gAsrStop.clear()
                gAudioBuffer.queue.clear()
                gResultBuffer.queue.clear()
                gThreadPool.clear()
                p_ctc = Thread(target=asr_ctc_thread, args=(language, ), daemon=True)
                p_mic = Thread(target=asr_mic_thread, args=(isUsingMic, audio_data), daemon=True)

                gThreadPool.append(p_mic)
                gThreadPool.append(p_ctc)
                p_ctc.start()
                p_mic.start()

                gOnlineThreadRunning = True

            try:
                utt = gResultBuffer.get(block = True, timeout = 2)
            except queue.Empty:
                utt = ''
            return return_message(0, utt, start_time)

        if realtime == 'ONLINE_STOP':
            gAsrStop.set()
            for p in gThreadPool:
                p.join()
            gThreadPool.clear()
            gOnlineThreadRunning = False
            message = 'Stop online mode successfully'
            return return_message(0, message, start_time)

def application(environ, start_response):

    result = get_result(environ)
    body = result
    status = '200 OK'
    headers = [('Content-Type', 'text/plain')]
    start_response(status, headers)

    return [body]

if __name__ == '__main__':
    WSGIServer(application).run()
