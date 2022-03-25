#!/bin/bash
# Copyright (C) 2021 Intel Corporation

set -ex

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi


DESTDIR=$(readlink -e $1)

cd "${0%/*}"
make

DEST_LIBDIR=$DESTDIR/usr/lib/x86_64-linux-gnu
mkdir -p $DEST_LIBDIR
cp -pf libccai_log.so $DEST_LIBDIR

DEVEL_ROOTFS=$DESTDIR/../devel_rootfs
mkdir -p $DEVEL_ROOTFS/usr/include
cp -pf ccai_log.h $DEVEL_ROOTFS/usr/include

#make clean
