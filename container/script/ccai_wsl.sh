#!/bin/bash

# Copyright (C) 2020 Intel Corporation

install_docker() {
    # add apt repo
    if ! apt-cache show docker-ce > /dev/null 2>&1; then
        local docker_repo='https://download.docker.com/linux/ubuntu'
        curl -fsSL $docker_repo/gpg | apt-key add - &&
        add-apt-repository "deb [arch=amd64] $docker_repo $(lsb_release -cs) stable" || exit
    fi

    # install docker-ce package and dependencies
    if ! dpkg -s docker-ce > /dev/null 2>&1; then
        apt-get install docker-ce &&
        gpasswd -a $USER docker || exit
    fi
}

start_service() {
    # start docker daemon
    if ! service docker status > /dev/null 2>&1; then
        service docker start || exit
        sleep 1
    fi

    # start syslog daemon
    if ! service rsyslog status > /dev/null 2>&1; then
        service rsyslog start
    fi
}

# parse arguments
model_dir="$HOME/models"
name='service_runtime_container'
while [[ "${1::1}" == '-' ]]; do
    case "${1#-}" in
        f) file=$2; shift;;
        m) model_dir=$2; shift;;
        n) name=$2; shift;;
        *) echo "unknown option: $1"; exit 1;;
    esac
    shift
done
if [[ "$1" == 'stop' && -n "$name" ]]; then
    docker stop "$name"
    exit
fi
if [[ -z "$1" && -t 0 && -z "$file" ]]; then 
    echo "$0 [-m model_dir] [-n name] image_id"
    echo ' or'
    echo "$0 [-m model_dir] [-n name] < archive"
    exit
fi
image=$1

# initialize if not
if ! service docker status > /dev/null 2>&1 ||
   ! service rsyslog status > /dev/null 2>&1; then
    if ! (( $(id -u) )); then
        install_docker
        start_service
        exit
    else
        sudo -Ehttps_proxy $0 "$@" || exit
    fi
fi

# load image if not
if [[ -z "$image" ]]; then
    while read -r; do
        echo "$REPLY"
    done < <(docker load ${file:+-i "$file"} 2>&1)
    image=${REPLY#Loaded image ID: }
    image=${REPLY#Loaded image: }
    (( ${#image} < ${#REPLY} )) || exit 1
    [[ "${image::7}" == "sha256:" ]] && image=${image:7:12}
elif ! docker images --format "# {{.ID}} {{.Repository}}:{{.Tag}} #" | grep -q " $image[: ]"; then
    echo "image not found"
    exit 1
fi

# run container
publish() {
    while (( $# )); do
        echo -n "-p 0.0.0.0:$1:$1 "
        shift
    done
}

mount_bind() {
    while (( $# )); do
        echo -n "--mount type=bind,src=${1%:*},dst=${1#*:} "
        shift
    done
}

group_add() {
    while (( $# )); do
        local e=$(getent group $1)
        e=${e%:*}
        echo -n "--group-add ${e##*:} "
        shift
    done
}

#opt_gpu="$(group_add video render) -e cl_cache_dir=/opt/fcgi/cgi-bin/models/cl_cache"
#opt_health="--health-cmd='cd /opt/health_monitor && python3 service_runtime_health_monitor_client.py 0' --health-interval=60s"
opt_readonly="--read-only --tmpfs /tmp --tmpfs /run"
opt_models="$(mount_bind $model_dir:/opt/fcgi/cgi-bin/models)"
opt_syslog="$(mount_bind /dev/log)"
opt_publish=$(publish 8080 8081 50006 50007)
opt_system="--init --cap-add SYS_ADMIN --ulimit core=0"
opt_general="--init --rm -d ${name:+--name $name}"
if [[ "$(docker ps --format '{{.Names}}')" == "$name" ]]; then
    echo "container \"$name\" is running"
    exit
fi
docker run $opt_general $opt_system $opt_readonly $opt_publish $opt_gpu $opt_syslog $opt_models $image
