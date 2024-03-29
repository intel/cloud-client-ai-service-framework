# Copyright (C) 2020 Intel Corporation

TAG := v1_$(shell date +%Y%m%d_%H%M%S)
DOCKER_IMAGE := service_runtime:$(TAG)
BASE_IMAGE   := service_runtime_base:ubuntu_20.04
OPENVINO_IMAGE   := service_runtime_inference_engine:ubuntu_20.04
DEMO_IMAGE   := service_runtime_demo
DEVEL_IMAGE   := service_runtime_devel

WORK_DIR = $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))

image: docker/Dockerfile.image app_rootfs service_runtime
	printf "%s" $(TAG) > service_runtime/docker_image_tag
	cp -a service_runtime/docker_image_tag docker/app_rootfs/
	../docker_build.sh .config docker/ $< $(DOCKER_IMAGE) \
		--iidfile service_runtime/docker_image
	docker tag $(DOCKER_IMAGE) service_runtime:latest

image_only: docker/Dockerfile.image
	../docker_build.sh .config docker/ $< $(DOCKER_IMAGE) \
		--iidfile service_runtime/docker_image
	docker tag $(DOCKER_IMAGE) service_runtime:latest

devel_image: docker/Dockerfile.devel
	ln -sf app_rootfs docker/devel_rootfs
	../docker_build.sh .config docker/ $< $(DEVEL_IMAGE)

demo_image: docker/Dockerfile.demo demo_rootfs
	../docker_build.sh .config docker/ $< $(DEMO_IMAGE)

inference_engine_image: docker/Dockerfile.inference_engine
	../docker_build.sh .config docker/ $< $(OPENVINO_IMAGE)

base_image: docker/Dockerfile.base_image
	../docker_build.sh .config docker/ $< $(BASE_IMAGE)

docker/Dockerfile.%: ../../docker/Dockerfile.%
	mkdir -p docker/
	cp -a $^ $@

demo_rootfs:
	rm -rf docker/demo_rootfs/
	mkdir -p docker/demo_rootfs/
	../../../gateway-demo/install.sh $(abspath docker/demo_rootfs/)

app_rootfs:
	mkdir -p docker/
	cmake -DCMAKE_INSTALL_PREFIX=docker/app_rootfs \
	      -DBUILD_CONFIG:PATH=$(WORK_DIR)/.config -B build ../../..
	make -C build install

	# openvino_2021 dose not create the link, '/opt/intel/openvino'.
	# Create the link in rootfs to avoid to modify app
	mkdir -p docker/app_rootfs/opt/intel/

service_runtime:  #TODO: convert shell scripts to cmake scripts
	mkdir -p service_runtime
	../../../api-gateway/install_to_service_runtime.sh $(WORK_DIR)/service_runtime $(WORK_DIR)/build
	../../../health-monitor/install_to_service_runtime.sh $(WORK_DIR)/service_runtime
	../../../service_runtime_binary/install_to_service_runtime.sh $(WORK_DIR)/service_runtime

release_dockerfile:
	rm -rf service_runtime_dockerfile_release/
	mkdir -p service_runtime_dockerfile_release
	cp -a docker/ service_runtime_dockerfile_release/
	cp -a service_runtime/ service_runtime_dockerfile_release/
	rm service_runtime_dockerfile_release/service_runtime/docker_image
menuconfig:
	MENUCONFIG_STYLE="monochrome" menuconfig ../config/Kconfig
defconfig:
	defconfig --kconfig ../config/Kconfig  ../config/defconfig

clean:
	rm -rf docker/ build service_runtime

debug:
	echo $(DOCKER_IMAGE)

project_%:
	sh -c "make -C build $(subst project_,,$@) || make -C build help"

.PHONY: image image_only devel_image demo_image inference_engine_image \
        base_image demo_rootfs app_rootfs service_runtime release_dockerfile \
        menuconfig defconfig clean debug
