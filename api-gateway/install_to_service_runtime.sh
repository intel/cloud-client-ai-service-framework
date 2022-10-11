#!/bin/bash

if [ $# != 2 ]; then
	echo "Usage: ./install_to_service_runtime.sh SERVICE_RUNTIME_DIR BUILD_DIR"
	exit 1
fi

set -xe

SERVICE_RUNTIME_DIR=$1
BUILD_DIR=$2
SCRIPT_DIR=$(dirname $(readlink -f $0))

pushd $SCRIPT_DIR

# prepare smartphoto dir in service_runtime
mkdir -p $SERVICE_RUNTIME_DIR/smartphoto/photos
mkdir -p $SERVICE_RUNTIME_DIR/smartphoto/thumbnail
chmod 777 -R $SERVICE_RUNTIME_DIR/smartphoto

# RESTful API test
mkdir -p $SERVICE_RUNTIME_DIR/test-script/
cp -a fcgi/test-script/* $SERVICE_RUNTIME_DIR/test-script/

for var in `find . -name python`
do
    dir=${var%python}
    dir=${dir#./fcgi}
    dir=$SERVICE_RUNTIME_DIR/test-script/test-demo/$dir

    if [ -f $var/post_local*.py ]; then
        mkdir -p $dir
        cp $var/post_*.py $dir
    fi
done

for var in `find . -name cpp`
do
    dir=${var%cpp}
    dir=${dir#./fcgi}
    dir=$SERVICE_RUNTIME_DIR/test-script/test-demo/$dir

    if [ -f $var/post_local*.py ]; then
        mkdir -p $dir
        cp $var/post_*.py $dir
    fi
done
if [ -d $SERVICE_RUNTIME_DIR/test-script/test-demo/tts/ ]; then
    cp -a fcgi/utils/python/fcgi_utils.py $SERVICE_RUNTIME_DIR/test-script/test-demo/tts/
fi
# gRPC API test
mkdir -p $SERVICE_RUNTIME_DIR/test-script/test-grpc/
cp -a $BUILD_DIR/api-gateway/grpc/*.py $SERVICE_RUNTIME_DIR/test-script/test-grpc/
cp -a grpc/grpc_inference_service_test.py \
	$SERVICE_RUNTIME_DIR/test-script/test-grpc/
popd
