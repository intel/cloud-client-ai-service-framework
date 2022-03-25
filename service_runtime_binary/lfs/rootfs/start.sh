#!/bin/bash

sv_stop() {
	for s in $(ls -d /etc/service/*); do
		sv stop $s
	done
}

trap "sv_stop; exit" SIGTERM

echo "AI-Service-Framework starting ..."
openvino_folder="$(basename $(readlink -e /opt/intel/openvino))"
echo "AI-Service-Framework OpenVINO version: ${openvino_folder#openvino_}"

for service in /etc/sv/*/; do
	service_name=$(basename $service)
	mkdir -p /tmp/$service_name/supervise
done

mkdir -p /tmp/lighttpd/uploads/

runsvdir /etc/service &
wait
