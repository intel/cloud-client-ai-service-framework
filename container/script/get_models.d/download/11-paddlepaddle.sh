#!/bin/bash

set -xe

cache_dir=$1
output_dir=$cache_dir/paddlepaddle
mkdir -p $output_dir
pushd $output_dir

wget -c https://paddle-imagenet-models-name.bj.bcebos.com/dygraph/inference/ResNet18_infer.tar
rm -rf ResNet18_infer
tar xvf ResNet18_infer.tar

popd
