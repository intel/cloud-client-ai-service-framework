#!/bin/bash

set -xe

script_dir=$(readlink -e $(dirname $0))
cache_dir=$1
output_dir=$cache_dir/misc/
mkdir -p $output_dir

pushd $output_dir

wget -c https://github.com/timesler/facenet-pytorch/releases/download/v2.2.9/20180402-114759-vggface2.pt
wget -c https://storage.googleapis.com/download.tensorflow.org/models/inception_v3_2016_08_28_frozen.pb.tar.gz
tar xvf inception_v3_2016_08_28_frozen.pb.tar.gz
wget -c https://raw.githubusercontent.com/pytorch/hub/master/imagenet_classes.txt -O resnet18.txt
wget -c https://github.com/openvinotoolkit/open_model_zoo/blob/4db5a93f17627ca5efaeccaa97f8a06af6ba4417/data/dataset_classes/scut_ept.txt -O scut_ept_char_list.txt
wget -c https://github.com/xmba15/onnx_runtime_cpp/blob/master/data/version-RFB-640.onnx
wget -c https://storage.openvinotoolkit.org/repositories/open_model_zoo/2022.1/models_bin/3/formula-recognition-medium-scan-0001/formula-recognition-medium-scan-0001-im2latex-encoder/vocab.json
python3 $script_dir/get_resnet18.py

wget -c https://raw.githubusercontent.com/openvinotoolkit/open_model_zoo/master/data/dataset_classes/imagenet_2012.txt
python3 $script_dir/imagenet_2012_txt_to_labels.py

popd
