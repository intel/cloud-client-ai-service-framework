#!/bin/bash

set -xe

script_dir=$(readlink -e $(dirname $0))
current_dir=$(pwd -P)
destdir=$current_dir/service_runtime/models/
workdir=$current_dir/get_models_cache/
omz_cache_dir=$workdir/omz_cache/
mkdir -p $destdir
mkdir -p $omz_cache_dir

download_script=$(find $script_dir/get_models.d/download/ -name "*.sh" | sort)
convert_script=$(find $script_dir/get_models.d/convert/ -name "*.sh" | sort)
install_script=$(find $script_dir/get_models.d/install/ -name "*.sh" | sort)

download_models="
face-detection-adas-0001
facial-landmarks-35-adas-0002
emotions-recognition-retail-0003
head-pose-estimation-adas-0001
human-pose-estimation-0007
text-detection-0003
text-recognition-0012
formula-recognition-medium-scan-0001
handwritten-simplified-chinese-recognition-0001
single-image-super-resolution-1032
alexnet
face-recognition-resnet100-arcface-onnx
hrnet-v2-c1-segmentation
deeplabv3
mobilenet-ssd
open-closed-eye-0001
"

convert_models="
alexnet
face-recognition-resnet100-arcface-onnx
hrnet-v2-c1-segmentation
deeplabv3
mobilenet-ssd
open-closed-eye-0001
"

copy_from_local_files="
/opt/intel/dlstreamer/samples/model_proc/intel/emotions-recognition-retail-0003.json
"

downloader="omz_downloader"
converter="omz_converter"

run_download_script()
{
	for script in $download_script; do
		$script $workdir
	done
}

run_convert_script()
{
	for script in $convert_script; do
		$script $workdir
	done
}

run_install_script()
{
	for script in $install_script; do
		$script $workdir $destdir
	done
}


download_and_convert()
{
	for model in $download_models; do
		echo $model
		if [ "x${model:0:1}" = "x#" ]; then
			continue
		fi
		$downloader --cache_dir $omz_cache_dir  --precisions FP32 --name $model
	done
	for model in $convert_models; do
		echo $model
		if [ "x${model:0:1}" = "x#" ]; then
			continue
		fi
		$converter -d $workdir --precisions FP32 --name $model
	done

	run_download_script
	run_convert_script
}

do_install()
{
	for local_file in $copy_from_local_files; do
		echo $local_file
		if [ "x${local_file:0:1}" = "x#" ]; then
			continue
		fi
		cp -a $local_file $destdir/
	done

	for bin_file in $(find $workdir/public -name "*.xml" -o -name "*.bin" | grep FP32); do
		cp -a $bin_file $destdir/
	done
	for bin_file in $(find $workdir/intel -name "*.xml" -o -name "*.bin" | grep FP32); do
		cp -a $bin_file $destdir/
	done

	run_install_script
}

cd $workdir

if [ "x$1" != "x--install-only" ] && [ "x$1" != "x-i" ]; then
	download_and_convert
fi
do_install
