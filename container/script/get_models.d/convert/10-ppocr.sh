#!/bin/bash

set -xe

cache_dir=$1
output_dir=$cache_dir/ppocr/
mkdir -p $output_dir
pushd $output_dir




mo --input_model  ./ch_ppocr_mobile_v2.0_det_infer/inference.pdmodel --output_dir . --data_type FP32 --input_shape [1,3,640,640] --mean_values [123.675,116.28,103.53] --scale_values [58.395,57.12,57.375]

mv ./inference.bin ./det.bin
mv ./inference.xml ./det.xml


counter=1
while [ $counter -lt 13 ]
do
    width=`expr 100 \* $counter`

    mo --input_model ./ch_ppocr_mobile_v2.0_rec_infer/inference.pdmodel --output_dir . --data_type FP32 --input_shape [1,3,32,$width] --static_shape --mean_values [123.675,116.28,103.53] --scale_values [58.395,57.12,57.375]

    new_bin_name="rec_$width.bin"
    new_xml_name="rec_$width.xml"
    echo $new_bin_name
    mv ./inference.bin ./$new_bin_name
    mv ./inference.xml ./$new_xml_name
    let counter+=1
done
