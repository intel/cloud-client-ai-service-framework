#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -xe

if [ $# != 1 ]; then
	echo "Usage: link_writable_folder.sh <rootfs>"
	exit 1
fi


ROOTFS=$(realpath $1)

pushd $ROOTFS

for service in etc/sv/*/; do
	service_name=$(basename $service)
	ln -sf /tmp/$service_name/supervise $service/supervise
done

popd
