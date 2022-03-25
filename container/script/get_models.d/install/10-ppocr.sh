#!/bin/bash

set -xe

cache_dir=$1
dest_dir=$2
script_dir=$(readlink -e $(dirname $0))

mkdir -p $dest_dir/ppocr

cp -a $cache_dir/ppocr/*.xml $dest_dir/ppocr/
cp -a $cache_dir/ppocr/*.bin $dest_dir/ppocr/
cp -a $cache_dir/ppocr/ppocr_keys_v1.txt $dest_dir/ppocr/

