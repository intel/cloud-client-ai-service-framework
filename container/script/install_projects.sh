#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -xe

if [ $# -lt 4 ]; then
	echo "Usage: install_projects.sh <project_dir> <rootfs> <service_runtime_dir> <configfile>"
	exit 1
fi


SCRIPT_DIR=$(cd $(dirname $0) >/dev/null 2>&1; pwd -P)
PROJECT_LIST=${SCRIPT_DIR}/project_list

PROJECT_DIR=$(realpath $1)
ROOTFS=$(realpath $2)
SERVICE_RUNTIME_DIR=$(realpath $3)
CONFIG_FILE=$(realpath $4)

PROJECT=$5

. $CONFIG_FILE
export $(grep '^CONFIG_' $CONFIG_FILE | cut -d= -f1 | xargs echo -n)


mkdir -p $ROOTFS
mkdir -p $SERVICE_RUNTIME_DIR

pushd $PROJECT_DIR

function install_project()
{
	project=$1

	if [ "x$project" = "x" ]; then
		return -1
	fi

	pushd $project
	if [ -x ./install.sh ]; then
		./install.sh $ROOTFS
	fi

	if [ -x ./install_to_service_runtime.sh ]; then
		./install_to_service_runtime.sh  $SERVICE_RUNTIME_DIR
	fi
	popd
	return 0
}

if [ "x$PROJECT" != "x" ]; then
	install_project $PROJECT
	exit 0
fi

for PROJECT in $(cat ${PROJECT_LIST}); do
	echo $PROJECT
	if [ "x$PROJECT" = "x" ]; then
		continue
	fi
	install_project $PROJECT
done

popd
exit 0
