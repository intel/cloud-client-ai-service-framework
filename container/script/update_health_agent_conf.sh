#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -e

if [ ! -e "service_runtime.sh" ]; then
	echo "error: this script should run in the 'service_runtime' folder."
	exit 1
fi

mkdir -p ./rootfs/f/opt/health_monitor

cp ../health-monitor/health-monitor-agent/config.yml.example  \
	./rootfs/f/opt/health_monitor/config.yml

sudo chmod 644 ./rootfs/f/opt/health_monitor/config.yml

./service_runtime.sh restart
