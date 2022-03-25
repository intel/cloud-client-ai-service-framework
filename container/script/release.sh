#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -xe

SCRIPT_DIR=$(dirname $(readlink -e $0))

VERSION=$1

if [ "x$VERSION" = "x" ]; then
	GIT_VERSION="$(git log -1 --format=%h HEAD)""$([[ -z $(git status -s) ]] || echo _dirty)"
	VERSION=$(date +%Y%m%d)_$GIT_VERSION
fi

# build / insall nesessary project
SERVICE_RUNTIME_DIR=$SCRIPT_DIR/integration_workdir/service_runtime/
pushd $SCRIPT_DIR/integration_workdir/health-monitor/
./install_to_service_runtime.sh $SERVICE_RUNTIME_DIR
popd
pushd $SCRIPT_DIR/integration_workdir/simlib/
./install_to_service_runtime.sh $SERVICE_RUNTIME_DIR
popd

RELEASE_VERSION=ccaisf_release_$VERSION
RELEASES_DIR=$SCRIPT_DIR/integration_workdir/release
CURRENT_RELEASE=$RELEASES_DIR/$RELEASE_VERSION
PACKAGE_DIR=$CURRENT_RELEASE/package

rm -rf $CURRENT_RELEASE
mkdir -p $CURRENT_RELEASE/
mkdir -p $PACKAGE_DIR/

# copy file to CURRENT_RELEASE
# release_build.sh
cp -a $SCRIPT_DIR/release_build.sh $CURRENT_RELEASE/
cp -a $SCRIPT_DIR/docker_build.sh $CURRENT_RELEASE/
cp -a $SCRIPT_DIR/integration_workdir/.config $CURRENT_RELEASE/

# the docker files
cp -a $SCRIPT_DIR/../docker $CURRENT_RELEASE/
# app rootfs
cp -a $SCRIPT_DIR/integration_workdir/docker/app_rootfs $CURRENT_RELEASE/docker/

# devel rootfs
cp -a $SCRIPT_DIR/integration_workdir/docker/devel_rootfs $CURRENT_RELEASE/docker/

# demo rootfs
cp -a $SCRIPT_DIR/integration_workdir/docker/demo_rootfs $CURRENT_RELEASE/docker/

# service_runtime
cp -a $SERVICE_RUNTIME_DIR $PACKAGE_DIR/

# clean some unnecessary files
rm -rf $PACKAGE_DIR/service_runtime/models/ELE_M.raw
rm -rf $PACKAGE_DIR/service_runtime/test-script/input_data/input.ark

# rm models
rm -rf $PACKAGE_DIR/service_runtime/models/*.pt
rm -rf $PACKAGE_DIR/service_runtime/models/*.onnx
rm -rf $PACKAGE_DIR/service_runtime/models/*.pb
rm -rf $PACKAGE_DIR/service_runtime/models/resnet18.txt
rm -rf $PACKAGE_DIR/service_runtime/models/imagenet_slim_labels.txt


cd $PACKAGE_DIR
mkdir -p models
mv service_runtime/models/* models
mkdir -p service_runtime/models/cl_cache/
mv models/cl_cache/readme.txt service_runtime/models/cl_cache/

cd $SCRIPT_DIR/integration_workdir/release/

rm -f ${RELEASE_VERSION}.tar.gz
rm -f ${RELEASE_VERSION}.tar.gz.md5


tar zcvf ${RELEASE_VERSION}.tar.gz $RELEASE_VERSION/
md5sum ${RELEASE_VERSION}.tar.gz > ${RELEASE_VERSION}.tar.gz.md5


#dpkg-buildpackage -us -uc -ui -i
