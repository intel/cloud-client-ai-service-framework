# Copyright (C) 2021 Intel Corporation

from ctypes import cdll

cs = cdll.LoadLibrary("libccai_stream.so")
cs.ccai_stream_init()

def _to_b(s):
    if s == None:
        return None
    else:
        return s.encode('utf-8')

class pipeline:
    def __init__(self, name, user_data=None):
        self.handle = cs.ccai_stream_pipeline_create(_to_b(name),
                _to_b(user_data))
        if not self.handle:
            raise Exception("failed to create pipieline: %s" % name)
            return
        self.name = name

    def __del__(self):
        if self.handle == 0:
            return
        cs.ccai_stream_pipeline_remove(self.handle, None)

    def start(self, user_data=None):
        cs.ccai_stream_pipeline_start(self.handle, _to_b(user_data))

    def stop(self, user_data=None):
        cs.ccai_stream_pipeline_stop(self.handle, _to_b(user_data))

