#!/bin/bash

set -xe

cache_dir=$1
lspeech_dir=$cache_dir/lspeech_s5_ext/FP32
mkdir -p $lspeech_dir
pushd $lspeech_dir

wget -c https://storage.openvinotoolkit.org/models_contrib/speech/2021.2/librispeech_s5/OV/hclg.fst
wget -c https://storage.openvinotoolkit.org/models_contrib/speech/2021.2/librispeech_s5/OV/labels.bin
wget -c https://storage.openvinotoolkit.org/models_contrib/speech/2021.2/librispeech_s5/OV/lspeech_s5_ext_v10.bin -O lspeech_s5_ext.bin
wget -c https://storage.openvinotoolkit.org/models_contrib/speech/2021.2/librispeech_s5/OV/lspeech_s5_ext_v10.xml -O lspeech_s5_ext.xml
wget -c https://storage.openvinotoolkit.org/models_contrib/speech/2021.2/librispeech_s5/OV/lspeech_s5_ext.feature_transform
wget -c https://storage.openvinotoolkit.org/models_contrib/speech/2021.2/librispeech_s5/OV/speech_recognition_config.template

popd
