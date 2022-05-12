#!/bin/bash

set -xe

cache_dir=$1
src_dir=$cache_dir/misc/
dest_dir=$2

mkdir -p $dest_dir

files="
20180402-114759-vggface2.pt
inception_v3_2016_08_28_frozen.pb
imagenet_slim_labels.txt
resnet18.pt
resnet18.txt
scut_ept_char_list.txt
version-RFB-640.onnx
vocab.json
alexnet.labels
"

for f in $files; do
	cp -a $src_dir/$f $dest_dir
done
