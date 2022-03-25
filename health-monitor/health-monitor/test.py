#!/usr/bin/env python3

# Copyright (C) 2020 Intel Corporation

import sys
import os
import socket
import struct
import grpc

from service_runtime_health_monitor_pb2 import AgentStatus
from service_runtime_health_monitor_pb2 import Empty
from service_runtime_health_monitor_pb2 import PulseServerAddr
from service_runtime_health_monitor_pb2_grpc import MonitorStub

def run(addr):
    with grpc.insecure_channel(addr) as channel:
        stub = MonitorStub(channel)
        status = AgentStatus()
        status.dead_targets.extend(['p1', 'p2'])
        status.new_dead_targets.extend(['p1',])
        stub.notify(status)

        empty = Empty()
        addr = stub.get_pulse_server_addr(empty)
        print(addr.status)
        print(addr.addr)


def get_default_gateway():
    with open("/proc/net/route") as proc_route:
        for line in proc_route:
            items = line.strip().split()
            if items[1] != '00000000' or not int(items[3], 16) & 2:
                continue
            return socket.inet_ntoa(struct.pack("<L", int(items[2], 16)))
    return None

if __name__ == '__main__':
     if len(sys.argv) < 2:
         gateway = get_default_gateway()
         os.environ["no_proxy"] = gateway
         addr = gateway + ':8082'
     else:
         addr = sys.argv[1]
     run(addr)
