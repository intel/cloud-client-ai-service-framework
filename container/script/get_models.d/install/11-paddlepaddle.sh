#!/bin/bash

set -xe

cache_dir=$1
dest_dir=$2

mkdir -p $dest_dir/paddlepaddle/
cp -a $cache_dir/paddlepaddle/ResNet18_infer $dest_dir/paddlepaddle/
chmod 644 $dest_dir/paddlepaddle/ResNet18_infer/*
