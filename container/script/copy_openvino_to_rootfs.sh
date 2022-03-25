#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -xe

if [ $# != 2 ]; then
	echo "Usage: ./copy_openvino_to_rootfs.sh <openvino> <rootfs>"
	exit 1
fi


OPENVINO_DIR=$(realpath $1)
ROOTFS=$(realpath $2)

OPT_INTEL_DIR=$ROOTFS/opt/intel

mkdir -p $OPT_INTEL_DIR
cp -a $OPENVINO_DIR $OPT_INTEL_DIR
cp -a $OPENVINO_DIR/../mediasdk $OPT_INTEL_DIR

pushd $OPT_INTEL_DIR
ln -sf $(basename $OPENVINO_DIR) openvino

# remove unnecessary files
rm -rf openvino/deployment_tools/tools
rm -rf openvino/deployment_tools/open_model_zoo
rm -rf openvino/inference_engine/samples
rm -rf mediasdk/share/mfx/samples
rm -rf openvino/deployment_tools/intel_models
rm -rf openvino/deployment_tools/inference_engine/demo

unstripped=$(find openvino/ -name "*.so*" -exec file {} \; | \
	grep "not stripped" | awk '{print $1}' | sed 's/://')
if [ "x$unstripped" != "x" ]; then
	strip $unstripped
fi

rm -rf openvino/data_processing/audio

pushd openvino/data_processing/dl_streamer
rm -rf debug
rm -rf include
rm -rf requirements.txt
rm -rf samples

pushd lib
rm libgstvideoanalyticsmeta.so
rm libgstvideoanalyticsmeta.so.1
ln -sf libgstvideoanalyticsmeta.so.1.1.0 libgstvideoanalyticsmeta.so
ln -sf libgstvideoanalyticsmeta.so.1.1.0 libgstvideoanalyticsmeta.so.1

rm libgstvideoanalytics.so
rm libgstvideoanalytics.so.1
ln -sf libgstvideoanalytics.so.1.1.0 libgstvideoanalytics.so
ln -sf libgstvideoanalytics.so.1.1.0 libgstvideoanalytics.so.1

rm libgvaitttracer.so
rm libgvaitttracer.so.1
ln -sf libgvaitttracer.so.1.1.0 libgvaitttracer.so
ln -sf libgvaitttracer.so.1.1.0 libgvaitttracer.so.1

rm libgvapython.so
rm libgvapython.so.1
ln -sf libgvapython.so.1.1.0 libgvapython.so
ln -sf libgvapython.so.1.1.0 libgvapython.so.1
popd # lib
popd # openvino/data_processing/dl_streamer
popd # $OPT_INTEL_DIR

# OpenCL driver
for deb in $(ls $OPT_INTEL_DIR/openvino/install_dependencies/*.deb); do
	dpkg -x $deb $ROOTFS
done
rm -rf $OPT_INTEL_DIR/openvino/install_dependencies/*.deb

# gva deps
ROOTFS_LIBDIR=$ROOTFS/usr/lib/x86_64-linux-gnu/
mkdir -p $ROOTFS_LIBDIR
pushd $ROOTFS_LIBDIR
ln -sf libglib-2.0.so.0.5600.4 libglib-2.0.so
ln -sf libgobject-2.0.so.0.5600.4 libgobject-2.0.so
popd
