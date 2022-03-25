#!/bin/bash

# Copyright (C) 2020 Intel Corporation

if [ $# != 1 ]; then
	echo "Usage: ./install_to_service_runtime.sh SERVICE_RUNTIME_DIR"
	exit 1
fi

set -xe

SERVICE_RUNTIME_DIR=$1
SCRIPT_DIR=$(dirname $(readlink -f $0))

pushd $SCRIPT_DIR

mkdir -p $SERVICE_RUNTIME_DIR/test-script/test-health-monitor/
cp -a health-monitor/test_health_monitor.sh \
	$SERVICE_RUNTIME_DIR/test-script/test-health-monitor/

rm -rf health-monitor/build/
mkdir -p health-monitor/build/
pushd health-monitor/build/

cmake ..
make -j4 VERBOSE=1
make DESTDIR=./install install/strip
cp ./install/opt/intel/service_runtime/service_runtime_health_monitor \
	$SERVICE_RUNTIME_DIR

mkdir -p $SERVICE_RUNTIME_DIR/debian

cp -a ../service_runtime_health_monitor.service \
	$SERVICE_RUNTIME_DIR/debian/service-runtime-health-monitor.service

popd

popd
