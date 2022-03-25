#!/bin/bash

if [ $# != 1 ]; then
	echo "Usage: ./install_to_service_runtime.sh SERVICE_RUNTIME_DIR"
	exit 1
fi

set -xe

SERVICE_RUNTIME_DIR=$1
SCRIPT_DIR=$(dirname $(readlink -f $0))

pushd $SCRIPT_DIR

cp -a service_runtime/service_runtime.sh $SERVICE_RUNTIME_DIR
cp -a service_runtime/ota/ $SERVICE_RUNTIME_DIR

mkdir -p $SERVICE_RUNTIME_DIR/debian/
cp -a service_runtime/service_runtime_debian/* $SERVICE_RUNTIME_DIR/debian/
cp -a service_runtime/service_runtime.service \
	$SERVICE_RUNTIME_DIR/debian/service-runtime.service

cp -a lfs/models/ $SERVICE_RUNTIME_DIR
mkdir -p $SERVICE_RUNTIME_DIR/models/debian/
cp -a service_runtime/models_debian/* $SERVICE_RUNTIME_DIR/models/debian/

mkdir -p $SERVICE_RUNTIME_DIR/test-script/
if [ -d lfs/test_data ]; then
	cp -a lfs/test_data $SERVICE_RUNTIME_DIR/test-script/input_data
fi

if [ -f lfs/cl_cache.tar.gz ]; then
	tar xf lfs/cl_cache.tar.gz -C $SERVICE_RUNTIME_DIR/models/cl_cache \
		--strip-components=1
	chmod 777 $SERVICE_RUNTIME_DIR/models/cl_cache/
	chmod 666 $SERVICE_RUNTIME_DIR/models/cl_cache/*
else
	mkdir -p $SERVICE_RUNTIME_DIR/models/cl_cache/
	chmod 777 $SERVICE_RUNTIME_DIR/models/cl_cache/
fi

cp service_runtime/service_runtime_devel.sh $SERVICE_RUNTIME_DIR/

# install desktop/icon file
if [ -f lfs/icon/CCAI.png ]; then
	cp -a lfs/icon/CCAI.png $SERVICE_RUNTIME_DIR/
fi
if [ -f lfs/icon/ccai.desktop ]; then
	cp -a lfs/icon/ccai.desktop $SERVICE_RUNTIME_DIR/
fi

popd
