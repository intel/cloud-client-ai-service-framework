#!/bin/bash

set -xe

cache_dir=$1
dest_dir=$2
script_dir=$(readlink -e $(dirname $0))

cp -a $cache_dir/lspeech_s5_ext $dest_dir/


mv $dest_dir/lspeech_s5_ext/FP32/speech_recognition_config.template \
	$dest_dir/lspeech_s5_ext/FP32/speech_lib.cfg

patch -Np1 $dest_dir/lspeech_s5_ext/FP32/speech_lib.cfg \
	< $script_dir/speech_lib.cfg.patch

