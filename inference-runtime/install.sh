#!/bin/bash

set -xe

if [ $# != 1 ]; then
	echo "Usage: ./install.sh DESTDIR"
	exit 1
fi

DESTDIR=$(realpath $1)
SCRIPT_DIR=$(cd $(dirname $0) >/dev/null 2>&1; pwd -P)

cd $SCRIPT_DIR

rm -rf release
rm -rf ie_runtime_build

pushd runtime_service/
source /opt/intel/openvino/bin/setupvars.sh
HOME=$(pwd)/../ ./build_runtime_lib.sh
popd

DEST_SBINDIR=$DESTDIR/usr/sbin
mkdir -p $DEST_SBINDIR
cp -a release/bin/policy_daemon $DEST_SBINDIR

DEST_LIBDIR=$DESTDIR/usr/lib/x86_64-linux-gnu
mkdir -p $DEST_LIBDIR
cp -a ./release/lib/*.so* $DEST_LIBDIR
cp -a ./release/lib/inference_engine_library.txt $DEST_LIBDIR

DEVEL_ROOTFS=$DESTDIR/../devel_rootfs
mkdir -p $DEVEL_ROOTFS/usr/include
cp ./release/include/* $DEVEL_ROOTFS/usr/include

# policy_setting.cfg
mkdir -p $DESTDIR/etc/
cp release/policy_setting.cfg $DESTDIR/etc/
cp release/lib/inference_engine_library.txt $DESTDIR/etc/

# runit service
mkdir -p $DESTDIR/etc/sv
cp -a runit/policy_daemon $DESTDIR/etc/sv
mkdir -p $DESTDIR/etc/runit/runsvdir/default
ln -sf /etc/sv/policy_daemon $DESTDIR/etc/runit/runsvdir/default/policy_daemon

# clean build dir
rm -rf ie_runtime_build
