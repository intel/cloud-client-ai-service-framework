#!/bin/bash

# Copyright (C) 2020 Intel Corporation

set -xe

SCRIPT_DIR=$(dirname $(readlink -f $0))
CONTAINER_NAME=service_runtime_container
SERVICE_RUNTIME_DIR=/opt/intel/service_runtime

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

container_status()
{
	running=$(docker inspect -f '{{.State.Running}}' $CONTAINER_NAME \
		2>/dev/null || echo "removed")
	case $running in
		true)
			echo "running"
			;;
		false)
			echo "stopped"
			;;
		*removed)
			echo "removed"
			;;
		*)
			echo "unknow status: $running"
			;;
	esac
}

wait_remove_container()
{
	retry=0
	while [ $retry -lt 5 ]; do
		if [ "$(container_status)" = "removed" ]; then
			return 0
		fi
		sleep 1
		retry=$(expr $retry + 1)
	done
	return 1
}

attach_device()
{
	device=$1
	if [ -e ${device} ]; then
		printf -- "--device %s " "${device}"
	fi
}

attach_devices()
{
	device_nodes=$1
	shopt -s nullglob
	for dev in /dev/${device_nodes}*; do
		printf -- "--device %s " "$dev"
	done
	shopt -u nullglob
}

add_groups()
{
	group_list="video render"
	for g in $group_list; do
		gid=$(getent group "$g" | awk -F: '{print $3}')
		if test "x$gid" != "x"; then
			printf -- "--group-add %s " "$gid"
		fi
	done
}

override_rootfs()
{
	rootfs=$SCRIPT_DIR/rootfs/d
	if test -d $rootfs; then
		pushd $rootfs > /dev/null
		find . -type f -name ".ccai-volume" |  sed 's|^./||' \
			| xargs dirname | while read -r d; do
			if test "$d" = "."; then
				continue
			fi
			printf -- "-v $rootfs/$d:/$d "
		done
		popd > /dev/null
	fi

	rootfs=$SCRIPT_DIR/rootfs/f
	if test -d $rootfs; then
		pushd $rootfs > /dev/null
		find . -type f | sed 's|^./||' | while read -r f; do
			printf -- "-v $rootfs/$f:/$f "
		done
		popd > /dev/null
	fi
}

mount_smartphoto()
{
	smartphoto=$SCRIPT_DIR/smartphoto
	if test -d $smartphoto; then
		printf -- "-v $smartphoto:/smartphoto "
	fi
}


add_service()
{
	if test -d "$SCRIPT_DIR"/service; then
		printf -- "-v "$SCRIPT_DIR"/service:/opt/intel/service_runtime/service"
	fi
}

xorg_opts()
{
	if test -d /tmp/.X11-unix; then
		printf -- "-v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY"
	fi
}


start_service_runtime()
{
	if test "$(container_status)" = "running"; then
		return 0
	fi
	wait_remove_container

	security_opts="--read-only"
	ulimit_core="0"

	while test "x$1" != "x"; do
		case $1 in
			-i | --image)
				shift
				DOCKER_IMAGE=$1
				;;
			-pr | --port-rest-api)
				shift
				port_rest_api=$1
				;;
			-pg | --port-grpc)
				shift
				port_grpc=$1
				;;
			-d | --debug)
				security_opts=""
				ulimit_core=-1
				debug_opts="-v $HOME:/mnt --privileged"
				;;
			-l | --limit-cpu-mem)
				limit_cpu_mem_opts="-m 4G --pids-limit 800"
				;;
			*)
				exit 1
		esac
		shift
	done

	if test "x${DOCKER_IMAGE}" = "x"; then
		if test -f $SCRIPT_DIR/docker_image; then
			DOCKER_IMAGE=$(cat $SCRIPT_DIR/docker_image)
		elif test -f $SERVICE_RUNTIME_DIR/docker_image; then
			DOCKER_IMAGE=$(cat $SERVICE_RUNTIME_DIR/docker_image)
		else
			echo "Cannot get docker image version"
			return 1
		fi
	fi

	if test "x${port_rest_api}" = "x"; then
		port_rest_api=8080
	fi

	if test "x${port_grpc}" = "x"; then
		port_grpc=8081
	fi

	ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "http_proxy")
	ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "https_proxy")
	ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "no_proxy")

	ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "HTTP_PROXY")
	ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "HTTPS_PROXY")
	ENV_OPTS=$(add_env_opts "${ENV_OPTS}" "NO_PROXY")


	docker run --name $CONTAINER_NAME --init --rm -d -it \
		--tmpfs "/tmp" --tmpfs "/run" \
		${security_opts} \
		$(add_groups) \
		$(override_rootfs) \
		$(mount_smartphoto) \
		-v /dev/log:/dev/log \
		$(attach_device /dev/dri) \
		$(attach_devices gna) \
		$(attach_devices video) \
		$(xorg_opts) \
		--mount type=bind,source="$SCRIPT_DIR"/models,target=/opt/fcgi/cgi-bin/models \
		$(add_service) \
		--health-cmd="cd /opt/health_monitor && python3 \
			service_runtime_health_monitor_client.py 0" \
		--health-interval=60s \
		--publish 127.0.0.1:$port_rest_api:8080 \
		--publish 127.0.0.1:$port_grpc:8081 \
                -e cl_cache_dir="/opt/fcgi/cgi-bin/models/cl_cache" \
		--ulimit core="${ulimit_core}" \
		--cap-add SYS_ADMIN \
		${debug_opts} \
		${limit_cpu_mem_opts} \
		${ENV_OPTS} \
		$DOCKER_IMAGE

	echo "service_runtime started"
}

stop_service_runtime()
{
	if [ "$(container_status)" = "running" ]; then
		docker stop $CONTAINER_NAME
	fi
	echo "service_runtime stoped"
}

case "$1" in
	start)
		shift
		start_service_runtime $@
		;;
	stop)
		stop_service_runtime
		;;
	restart)
		shift
		stop_service_runtime
		start_service_runtime $@
		;;

	*)
		echo "Usage: ./service_runtime.sh {start|stop|restart} [opts]"
		echo -e "\topts:"
		echo -e "\t\t--port-rest-api <rest serivce port>"
		echo -e "\t\t--port-grpc <grpc serivce port>"
		echo -e "\t\t--image <docker tag/id>"
		exit 1
esac

exit 0
