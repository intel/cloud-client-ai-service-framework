#!/bin/bash

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

DESTDIR=$1
SCRIPT_DIR=$(cd $(dirname $0) >/dev/null 2>&1; pwd -P)

cp -a $SCRIPT_DIR/lfs/rootfs/* $DESTDIR/

DEVEL_ROOTFS=$DESTDIR/../devel_rootfs
mkdir -p $DEVEL_ROOTFS

if [ -f $SCRIPT_DIR/lfs/speech_recognition.tar.gz ]; then
	tar xvf $SCRIPT_DIR/lfs/speech_recognition.tar.gz -C $DEVEL_ROOTFS
fi
