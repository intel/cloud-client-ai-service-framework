#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -xe

SCRIPT_DIR=$(dirname $(readlink -e $0))

if [ $# -lt 4 ]; then
	echo "Usage: ./docker_build.sh <configfile> <dockerfilefolder> <dockerfile> <imagename>"
	exit 1
fi

CONFIG_FILE=$1
DOCKER_FILE_FOLDER=$2
DOCKER_FILE=$3
IMAGE_NAME=$4
shift 4
OTHER_DOCKER_ARGS=$@

configs=""
if [ -f "${CONFIG_FILE}" ]; then
	configs=$(grep '^CONFIG_' ${CONFIG_FILE} | awk '$0="--build-arg "$0" "' | tr -d '\n')
else
	echo "${CONFIG_FILE} does not exist!"
	exit 1
fi

docker build --file ${DOCKER_FILE} \
	--build-arg http_proxy=${http_proxy} \
	--build-arg https_proxy=${https_proxy} \
	${configs} \
	-t ${IMAGE_NAME} \
	${OTHER_DOCKER_ARGS} \
	${DOCKER_FILE_FOLDER}
