#!/usr/bin/env bash

# Copyright (C) 2020 Intel Corporation
#

set -xe

RUNTIME_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

printf "\nSetting environment variables for building runtime library...\n"

if ! command -v cmake &>/dev/null; then
    printf "\n\nCMAKE is not installed. Please install it. \n\n"
    exit 1
fi

if [ -z "$INTEL_OPENVINO_DIR" ]; then
    printf "\nNot setting environment variables for openvino!\n"
    exit 1
fi

build_dir=$HOME/ie_runtime_build

OS_PATH=$(uname -m)
NUM_THREADS="-j2"

if [ "$OS_PATH" == "x86_64" ]; then
    OS_PATH="intel64"
    NUM_THREADS="-j8"
fi

if [ -e "$build_dir/CMakeCache.txt" ]; then
    rm -rf "$build_dir/CMakeCache.txt"
fi
mkdir -p "$build_dir"

(cd "$build_dir" && cmake -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
	-DCMAKE_BUILD_TYPE=Release "$RUNTIME_PATH" -DCMAKE_PREFIX_PATH=/opt/libtorch)
cmake --build "$build_dir" -- "$NUM_THREADS"

#Create release folders
if [ ! -d "../release" ]; then
    mkdir ../release
fi

if [ ! -d "../release/include" ]; then
    mkdir ../release/include
fi

if [ ! -d "../release/lib" ]; then
    mkdir ../release/lib
fi

if [ ! -d "../release/bin" ]; then
    mkdir ../release/bin
fi

cp -f $build_dir/intel64/Release/lib/libinferservice.so ../release/lib/
cp -f $build_dir/intel64/Release/lib/libonnxentry.so ../release/lib/
cp -f $build_dir/intel64/Release/lib/libopenvinoentry.so ../release/lib/
cp -f $build_dir/intel64/Release/lib/libpytorchentry.so ../release/lib/
cp -f $build_dir/intel64/Release/lib/libtensorflowentry.so ../release/lib/
cp -f $build_dir/intel64/Release/lib/libpaddleentry.so ../release/lib/
cp -f $build_dir/intel64/Release/lib/inferservice_python.*.so ../release/lib/
cp -f $build_dir/intel64/Release/policy_daemon ../release/bin/
cp -f $build_dir/intel64/Release/encrypt ../release/bin/
cp -f ./src/policy_setting.cfg ../release/
cp -f ./src/inference_engine_library.txt ../release/lib/
cp -f ./src/vino_ie_pipe.hpp ../release/include/
cp -f ./lib/* ../release/lib/

printf "\nBuild completed, you can find binaries in the %s subfolder.\n\n" "$build_dir/$OS_PATH/Release"
