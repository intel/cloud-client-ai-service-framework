#!/bin/bash
# Copyright (C) 2021 Intel Corporation

set -ex

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi


DESTDIR=$(readlink -f $1)
DEVEL_ROOTFS=$DESTDIR/../devel_rootfs

MYSCRIPT_DIR=$(readlink -e $(dirname $0))
pushd $MYSCRIPT_DIR/

. /opt/intel/openvino/bin/setupvars.sh
make DESTDIR=$DEVEL_ROOTFS install

popd

# install binary files to DESTDIR
dev_lib_DIR=$DEVEL_ROOTFS/usr/lib/x86_64-linux-gnu/
dev_plugin_DIR=$DEVEL_ROOTFS/usr/lib/ccai_stream/plugins
lib_DIR=$DESTDIR/usr/lib/x86_64-linux-gnu/
plugin_DIR=$DESTDIR/usr/lib/ccai_stream/plugins

mkdir -p $lib_DIR
mkdir -p $plugin_DIR
cp -a $dev_lib_DIR/libccai_stream.so $lib_DIR
cp -a $dev_plugin_DIR/* $plugin_DIR
