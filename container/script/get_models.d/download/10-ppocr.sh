#!/bin/bash

set -xe

cache_dir=$1
output_dir=$cache_dir/ppocr/
mkdir -p $output_dir
pushd $output_dir


wget -c https://github.com/PaddlePaddle/PaddleOCR/blob/release/2.4/ppocr/utils/ppocr_keys_v1.txt
wget -c https://paddleocr.bj.bcebos.com/dygraph_v2.0/ch/ch_ppocr_mobile_v2.0_rec_infer.tar
wget -c https://paddleocr.bj.bcebos.com/dygraph_v2.0/ch/ch_ppocr_mobile_v2.0_det_infer.tar
tar xvf ch_ppocr_mobile_v2.0_det_infer.tar
tar xvf ch_ppocr_mobile_v2.0_rec_infer.tar

popd
