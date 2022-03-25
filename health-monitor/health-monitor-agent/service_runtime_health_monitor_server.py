#Copyright (C) 2020 Intel Corporation
import grpc
import service_runtime_health_monitor_pb2
import service_runtime_health_monitor_pb2_grpc
from concurrent import futures
import time
import os

_ONE_DAY_IN_SECONDS = 60 * 60 * 24

class MsgServicer(service_runtime_health_monitor_pb2_grpc.MonitorServicer):

    def notify(self, request, context):
        print("Received dead_targets name: %s" % request.dead_targets)
        print("Received new_dead_targets name: %s" % request.new_dead_targets)
        print("Received dead_healthcheck_targets name: %s" % request.dead_healthcheck_targets)
        print("Received new_dead_healthcheck_targets name: %s" % request.new_dead_healthcheck_targets)
        return service_runtime_health_monitor_pb2.Empty()

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers = 10))
    service_runtime_health_monitor_pb2_grpc.add_MonitorServicer_to_server(MsgServicer(), server)
    server.add_insecure_port('localhost:60001')
    server.start()
    try:
        while True:
            time.sleep(_ONE_DAY_IN_SECONDS)
    except KeyboardInterrupt:
        server.stop(0)

if __name__ == '__main__':
    serve()
