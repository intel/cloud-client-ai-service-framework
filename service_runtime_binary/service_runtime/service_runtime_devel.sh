#!/bin/bash
# Copyright (C) 2020 Intel Corporation

set -xe

SCRIPT_DIR=$(dirname $(readlink -f $0))
IMAGE=$1


if [ "x$IMAGE" = "x" ]; then
	IMAGE=service_runtime_devel
fi

DOCKER_GID=`getent group | grep docker | awk -F: '{print $3}'`
SUDO_GID=`getent group | grep sudo | awk -F: '{print $3}'`

if [ "x$DOCKER_GID" = "x" ] || [ "x$SUDO_GID" = 'x' ]; then
	echo "error: cannot get docker gourp id or sudo group id"
	exit 1
fi

add_env_opts()
{
	env_opts=$1
	variable_name=$2
	eval value=\$$variable_name
	if test "x${value}" != "x" ; then
		env_opts="$env_opts -e ${variable_name}=${value}"
	fi
	echo "$env_opts"
}
ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "http_proxy")
ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "https_proxy")
ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "no_proxy")

docker run --name service_runtime_devel_container_${USER} --init -it -d \
	-u $(id -u ${USER}):$(id -u ${GOURP}) \
	--group-add ${DOCKER_GID} --group-add ${SUDO_GID} \
	--volume="/etc/group:/etc/group:ro" \
	--volume="/etc/passwd:/etc/passwd:ro" \
	--volume="/etc/shadow:/etc/shadow:ro" \
	--volume="/etc/sudoers.d:/etc/sudoers.d:ro" \
	--volume="/etc/sudoers:/etc/sudoers:ro" \
	--privileged \
	--tmpfs "/tmp" \
	${ENV_OPTS} \
	-v /dev:/dev \
	-v /dev/log:/dev/log \
	-v /tmp/.X11-unix:/tmp/.X11-unix \
	-v /var/run/docker.sock:/var/run/docker.sock \
	-v $HOME:$HOME \
	-v $SCRIPT_DIR/models:/opt/fcgi/cgi-bin/models \
	$IMAGE /start.sh
