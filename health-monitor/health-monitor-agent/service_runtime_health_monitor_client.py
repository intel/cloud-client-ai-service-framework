#Copyright (C) 2020 Intel Corporation
import grpc
import service_runtime_health_monitor_pb2
import service_runtime_health_monitor_pb2_grpc
import os
import time
import requests
import urllib.parse
from collections import OrderedDict
import yaml
import socket
import struct
import inference_service_pb2
import inference_service_pb2_grpc
import sys

def get_daemon_status(daemon_targets):
    all_status = os.popen('ps -ef | awk \'//{print $9}\' ').read()
    all_status = list(all_status.split())
    dead_daemon_status = []

    for target in daemon_targets:
        if target not in all_status:
            dead_daemon_status.append(target)
    return dead_daemon_status

def get_grpc_status(grpc_targets):
    grpc_status = os.popen('ps -ef | grep \'/usr/sbin/grpc\' | awk \'//{print  $8 " " $9}\' ').read()
    grpc_status = list(grpc_status.split('\n'))
    grpc_dead_targets = []
    for target in grpc_targets:
        target = "/usr/sbin/grpc_inference_service " + target.split(' ')[1]
        if target not in grpc_status:
            grpc_dead_targets.append(target)
    return grpc_dead_targets

def get_fcgi_status(fcgi_targets):
    fcgi_status = os.popen('ps -ef | grep cgi-bin | grep -v intel | grep -v grep | awk \'//{print $8 $9}\'').read()
    fcgi_status = list(fcgi_status.split())
    for i in range(len(fcgi_status)):
        fcgi_status[i] = fcgi_status[i].split("/cgi-bin/")[1]
    fcgi_dead_targets = []
    for target in fcgi_targets:
        if target not in fcgi_status:
            fcgi_dead_targets.append(target)

    return fcgi_dead_targets

def kill_fcgi(target):

    if 'py' in target:
        command = 'fcgi_' + target.split('py_')[1] + '.py'
        command = 'ps -ef | grep -v grep | grep ' + command + '| awk \'//{print $2}\''
        if os.popen(command).read() != '':
            command += ' | xargs kill -9'
            os.popen(command)

    else:
        command = 'ps -ef | grep -v grep | grep -v python3 | grep ' + target + '| awk \'//{print $2}\''
        if os.popen(command).read() != '':
            command += ' | xargs kill -9 '
            os.popen(command)

def kill_grpc (target):
    command = 'ps -ef |grep ' + target.split(' ')[1] + ' |grep -v grep | awk \'//{print $2}\''
    if os.popen(command).read() != '':
        command += '| xargs kill -9'
        os.popen(command)


def get_healthcheck_status(grpc_targets, fcgi_targets):
    dead_healthcheck_targets = set()
    for i in range(len(grpc_targets)):
        with grpc.insecure_channel('localhost:' +  grpc_targets[i].split(' ')[1]) as channel: 

            try:
                resp = inference_service_pb2.HealthStatus()
                resp.status =-1
                stub = inference_service_pb2_grpc.InferenceStub(channel)
                resp = stub.HealthCheck(inference_service_pb2.Empty(), timeout = 2)
                if (resp.status != 0):
                    dead_healthcheck_targets.add(grpc_targets[i].split(' ')[0])
                    kill_grpc(grpc_targets[i])

            except:
                dead_healthcheck_targets.add(grpc_targets[i].split(' ')[0])
                kill_grpc(grpc_targets[i])

    params = OrderedDict()
    params['healthcheck'] = 1

    healthcheck_targets = fcgi_targets.copy()

    for i in range(len(fcgi_targets)):
        if ".py" in fcgi_targets[i]:
            healthcheck_targets[i] = fcgi_targets[i].split('.')[0]
            healthcheck_targets[i] = healthcheck_targets[i].split('_')[0] + "_py_" + healthcheck_targets[i].split('_', 1)[1]

    for target in healthcheck_targets:
        try:
            url = 'http://localhost:8080/cgi-bin/' + target
            res = requests.post(url, data = params, timeout = 2)
            if res.text != "healthcheck ok":
                dead_healthcheck_targets.add(target)
                kill_fcgi(target)
        except:
            dead_healthcheck_targets.add(target)
            kill_fcgi(target)

    return dead_healthcheck_targets

def get_default_gateway():
    with open("/proc/net/route") as proc_route:
        for line in proc_route:
            items = line.strip().split()
            if items[1] != '00000000' or not int(items[3], 16) & 2:
                continue
            return socket.inet_ntoa(struct.pack("<L", int(items[2], 16)))
    return None
def run():
    mointor_server_addr = get_default_gateway()
    if mointor_server_addr == None:
        return

    os.environ["no_proxy"] = mointor_server_addr + ",localhost"

    with grpc.insecure_channel(mointor_server_addr + ':8082') as channel:
        stub = service_runtime_health_monitor_pb2_grpc.MonitorStub(channel)

        f = open('./config.yml')
        all_targets = yaml.safe_load(f)

        daemon_targets = all_targets['daemon_targets'] or []
        grpc_targets = all_targets['grpc_targets'] or []
        fcgi_targets = all_targets['fcgi_targets'] or []

        dead_daemon_status = get_daemon_status(daemon_targets)
        if len(dead_daemon_status) > 0:
            for status in dead_daemon_status:
                command = 'sv restart ' + status
                os.popen(command)
        grpc_dead_targets = get_grpc_status(grpc_targets)
        fcgi_dead_targets = get_fcgi_status(fcgi_targets)
        dead_targets = dead_daemon_status + grpc_dead_targets + fcgi_dead_targets
        dead_healthcheck_targets = get_healthcheck_status(grpc_targets, fcgi_targets)

        time.sleep(2)
        if len(dead_daemon_status) > 0:
            dead_daemon_status = get_daemon_status(daemon_targets)
        if len(grpc_dead_targets) > 0:
            grpc_dead_targets = get_grpc_status(grpc_targets)
        if len(fcgi_dead_targets) > 0:
            fcgi_dead_targets = get_fcgi_status(fcgi_targets)

        new_dead_targets = dead_daemon_status + grpc_dead_targets + fcgi_dead_targets
        new_dead_healthcheck_targets = get_healthcheck_status(grpc_targets, fcgi_targets)
        print("Received dead_targets name: %s" % dead_targets)
        print("Received new_dead_targets name: %s" % new_dead_targets)
        print("Received dead_healthcheck_targets name: %s" % dead_healthcheck_targets)
        print("Received new_dead_healthcheck_targets name: %s" % new_dead_healthcheck_targets)
        stub.notify(service_runtime_health_monitor_pb2.AgentStatus(dead_targets = dead_targets, new_dead_targets = new_dead_targets, dead_healthcheck_targets = dead_healthcheck_targets, new_dead_healthcheck_targets = new_dead_healthcheck_targets), timeout = 2)
        if (len(new_dead_targets) > 0) or (len(new_dead_healthcheck_targets) > 0):
            return 1
        else:
            return 0

if __name__ == '__main__':
    sleep_time = float(sys.argv[1])
    time.sleep(sleep_time)
    dead_target = run()

    if sleep_time == 0:
        sleep_time = 30

    if dead_target > 0 and sleep_time > 8:
        sleep_time = sleep_time / 2
        command = "python3 " + sys.argv[0] + " " + str(sleep_time)
        os.system(command)
    else:
        exit()
