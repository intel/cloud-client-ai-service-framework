#!/bin/bash
# Copyright (C) 2020 Intel Corporation

WORKDIR=/tmp/service_runtime_ota/
OTA_HOOK_SCRIPT=usr/bin/service_runtime_ota.sh
DEB_PACKAGE_DIR=${DEB_PACKAGE_DIR}

if [ "x$DEB_PACKAGE_DIR" = "x" ]; then
	DEB_PACKAGE_DIR=/var/cache/apt/archives/
fi

postdownload()
{
	set -xe
	workdir=$1
	echo "***"
	echo "start service_runtime_ota.sh"
	echo "***"
	echo "WORKDIR: $workdir"

	docker_image_prefix=intel/service_runtime:
	new_tag_file=opt/intel/service_runtime/docker_image_tag
	old_tag_file=/$new_tag_file

	tag_new=$(cat "$new_tag_file")
	if [ -e "$old_tag_file" ]; then
		tag_old=$(cat "$old_tag_file")
	fi

	echo "tag_new: $tag_new"
	echo "tag_old: $tag_old"

	if [ "$tag_new" != "$tag_old" ]; then
		docker pull ${docker_image_prefix}${tag_new}
		if [ $? -ne 0 ]; then
			exit 1
		fi
	fi

	rm -rf $workdir
	exit 0
}

extract_deb_and_do_v2_postdownload()
{
	deb_package_dir=$(readlink -e $DEB_PACKAGE_DIR)
	if [ ! -e $deb_package_dir ]; then
		echo "Cannot get deb package: $DEB_PACKAGE_DIR"
		exit 0
	fi
	# find service-runtime-xxx.deb
	deb_package=$(find $deb_package_dir -name "service-runtime_*-*_*.deb" \
		| sort --version-sort | tail -1)
	if [ "x$deb_package" = "x" ]; then
		echo "no service_runtime upgrade"
		exit 0
	fi

	mkdir -p $WORKDIR
	pushd $WORKDIR
	dpkg -x $deb_package ./
	ota_hook_script=$(readlink -e ./${OTA_HOOK_SCRIPT})
	if [ -e "$ota_hook_script" ]; then
		echo "run $ota_hook_script -pd $WORKDIR"
		exec /bin/bash $ota_hook_script -pd $WORKDIR
	else
		echo "WARNING: cannot find hook script: $ota_hook_script"
	fi
	popd
	rm -rf $WORKDIR
}

case "$1" in
	-pd|--postdownload)
		shift
		postdownload $@
		;;
	-h|--help)
		echo "Usage: ./service_runtime_ota.sh [-pd | -h]"
		echo -e "\topts:"
		echo -e "\t\t--pd|--postdownload WORKDIR"
		echo -e "\t\t--h |--help"
		;;
	*)
		echo "do_v2_postdownload"
		extract_deb_and_do_v2_postdownload
		;;
esac

exit 0
