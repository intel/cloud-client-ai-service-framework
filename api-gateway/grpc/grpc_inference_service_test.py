#!/usr/bin/env python3

# Copyright (C) 2020 Intel Corporation

import argparse
import base64
import sys
import grpc
import inference_service_pb2
import inference_service_pb2_grpc

ASR_INPUT='input_data/how_are_you_doing.wav'
CLASSIFICATION_INPUT='input_data/classification.jpg'
FACEDETECTION_INPUT='input_data/face-detection-adas-0001.png'
FACIAL_LANDMARK_INPUT='input_data/face-detection-adas-0001.png'
OCR_INPUT='input_data/intel.jpg'

class BasicAuthenticationPlugin(grpc.AuthMetadataPlugin):
    def __init__(self, username, password):
        super(BasicAuthenticationPlugin, self).__init__()
        if username:
            self.credential = username + ":" + password
        else:
            self.credential = None

    def __call__(self, context, callback):
        if self.credential:
            cred = base64.b64encode(self.credential.encode('utf-8'))
            header = ('authorization', 'Basic ' + cred.decode('ascii'))
            callback([header], None)
        else:
            callback(None, None)

def run(port, tls = False, username = None, password = None, rootcert = None):
    # create call credential
    metadata_plugin = BasicAuthenticationPlugin(username, password)
    call_cred = grpc.metadata_call_credentials(metadata_plugin)

    # create channel credential
    if tls == False:
        channel_cred = grpc.local_channel_credentials()
    elif rootcert:
        with open(rootcert, 'rb') as f:
            channel_cred = grpc.ssl_channel_credentials(f.read())
    else:
        channel_cred = grpc.ssl_channel_credentials()

    credentials = grpc.composite_channel_credentials(channel_cred, call_cred)
    with grpc.secure_channel('localhost:' + port, credentials) as channel:
        stub = inference_service_pb2_grpc.InferenceStub(channel)

        resp = stub.HealthCheck(inference_service_pb2.Empty())
        print('HealthCheck resp:', resp.status)

        def call_service(input_file, func_name):
            with open(input_file, 'rb') as in_file:
                func = getattr(stub, func_name)
                resp = func(inference_service_pb2.Input(buffer=in_file.read()))
                print(func_name + " result:\n\t" + resp.json)

        call_service(ASR_INPUT, "ASR")
        call_service(CLASSIFICATION_INPUT, "Classification")
        call_service(FACEDETECTION_INPUT, "FaceDetection")
        call_service(FACIAL_LANDMARK_INPUT, "FacialLandmark")
        call_service(OCR_INPUT, "OCR")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', action='store', dest='rootcert', help='root certificate file')
    parser.add_argument('-s', action='store_true', dest='tls', help='secure channel')
    parser.add_argument('-u', action='store', dest='username', help='username')
    parser.add_argument('-p', action='store', dest='password', help='password')
    parser.add_argument('port', action='store', nargs='?', default='8081')
    args = parser.parse_args()
    if args.rootcert:
        args.tls = True
    run(args.port, args.tls, args.username, args.password, args.rootcert)
