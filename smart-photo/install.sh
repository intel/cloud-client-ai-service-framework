#!/bin/bash
# Copyright (C) 2021 Intel Corporation

set -ex

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

ROOTFS=$(readlink -f $1)
SCRIPT_DIR=$(readlink -e $(dirname $0))
DEVEL_ROOTFS=$ROOTFS/../devel_rootfs
DEVEL_LIB_DIR=$DEVEL_ROOTFS/usr/lib/x86_64-linux-gnu/
LIB_DIR=$ROOTFS/usr/lib/x86_64-linux-gnu/

mkdir -p $DEVEL_LIB_DIR
mkdir -p $LIB_DIR

mkdir -p $SCRIPT_DIR/build
cd $SCRIPT_DIR/build

. /opt/intel/openvino_2022/setupvars.sh
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc --all) DESTDIR=$DEVEL_ROOTFS install/strip

cp -a $DEVEL_LIB_DIR/* $LIB_DIR/

