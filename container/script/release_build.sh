#!/bin/bash

# Copyright (C) 2020 Intel Corporation

SCRIPT_DIR=$(dirname $(readlink -e $0))

BASE_IMAGE=service_runtime_base:ubuntu_20.04
OPENVINO_IMAGE=service_runtime_inference_engine:ubuntu_20.04
DEVEL_IMAGE=service_runtime_devel:latest
DEMO_IMAGE=service_runtime_demo:latest

build_base_image()
{
	cd $SCRIPT_DIR
	./docker_build.sh .config docker/ docker/Dockerfile.base_image \
		$BASE_IMAGE
	exit 0
}

build_openvino_image()
{
	cd $SCRIPT_DIR
	./docker_build.sh .config docker/ docker/Dockerfile.inference_engine \
		$OPENVINO_IMAGE
	exit 0
}

build_image()
{
	cd $SCRIPT_DIR

	docker_image_tag_file=package/service_runtime/docker_image_tag
	docker_image_id=package/service_runtime/docker_image

	if [ -e $docker_image_tag_file ]; then
		TAG=$(cat $docker_image_tag_file)
	else
		TAG=v1_$(date +%Y%m%d_%H%M%S)
		printf "%s" ${TAG} > $docker_image_tag_file
	fi
	cp -a $docker_image_tag_file docker/app_rootfs/
	IMAGE=service_runtime:${TAG}

	./docker_build.sh .config docker/ docker/Dockerfile.image $IMAGE \
		 --iidfile $docker_image_id
	docker tag $IMAGE service_runtime:latest

	exit 0
}

build_devel_image()
{
	cd $SCRIPT_DIR
	./docker_build.sh .config docker/ docker/Dockerfile.devel $DEVEL_IMAGE
	exit 0
}

build_demo_image()
{
	cd $SCRIPT_DIR
	docker_image_id=package/service_runtime/docker_image

	./docker_build.sh .config docker/ docker/Dockerfile.demo $DEMO_IMAGE \
		 --iidfile $docker_image_id

	exit 0
}



build_package()
{
	cd $SCRIPT_DIR/package/service_runtime
	dpkg-buildpackage -us -uc -ui -i
	exit 0
}

build_models_package()
{
	cd $SCRIPT_DIR/package/models
	dpkg-buildpackage -us -uc -ui -i
	exit 0
}

case "$1" in
	b* | base_image*)
		build_base_image
		;;
	inf* | inference_engine_image)
		build_openvino_image
		;;
	im* | image)
		build_image
		;;
	dev* | devel_image)
		build_devel_image
		;;
	demo* | demo_image)
		build_demo_image
		;;
	p* | package)
		build_package
		;;
	m* | models_package)
		build_models_package
		;;
	*)
		printf "Usage: ./release_build.sh {base_image|"
		printf "inference_engine_image|image|devel_image|demo_image|package|models_package}\n"
		exit 1
esac
