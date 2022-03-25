#!/bin/bash

# Copyright (C) 2020 Intel Corporation
echo '*******************************'
echo 'test fcgi and grpc daemon:'
cgi_name=`ps -ef | grep www-data | grep /opt/fcgi/cgi-bin | awk '{print $8 $9}'`
cgi_pids=`ps -ef | grep www-data | grep /opt/fcgi/cgi-bin | awk '{print $2}'`

if [ "$cgi_pids" = "" ]
then
    echo 'no fcgi daemon'
else
    sudo kill  $cgi_pids
    sleep 6

    cgi_new_name=`ps -ef | grep www-data | grep /opt/fcgi/cgi-bin | awk '{print $8 $9}'`
    cgi_flag='1'
    for i in $cgi_name
    do
        if [[ ${cgi_new_name} =~ "${i}" ]]
        then
            continue
        else
            echo $i 'can not restart'
            cgi_flag='0'
        fi
    done
    if [ "$cgi_flag" = "1" ]
    then
        echo 'fcgi can automatic restart'
    else
        echo 'fcgi can not automatic restart'
    fi
fi

grpc_name=`ps -ef | grep /usr/sbin/grpc_inference_service | grep -v grep | awk '{print $8 $9}'`
grpc_pids=`ps -ef | grep /usr/sbin/grpc_inference_service | grep -v grep | awk '{print $2}'`
grpc_flag='1'
if [ "$grpc_pids" = "" ]
then
    echo 'no grpc daemon'
else
    sudo kill $grpc_pids
    sleep 6
    grpc_new_name=`ps -ef | grep /usr/sbin/grpc_inference_service | grep -v grep | awk '{print $8 $9}'`
    for i in $grpc_name
    do
        if [[ ${grpc_new_name} =~ "${i}" ]]
        then
            continue
        else
            echo $i 'can not restart'
            grpc_flag='0'
        fi
    done
    if [ "$grpc_flag" = "1" ]
    then
        echo 'grpc can automatic restart'
    else
        echo 'grpc can not automatic restart'
    fi
fi

echo '*******************************'
echo 'test container:'

container_status=`docker ps | grep service_runtime_container`
echo $container_status
if [ "$container_status" = "" ]
then
    echo 'container does not exist'
else
    docker stop service_runtime_container
    echo "restart container"
    echo "sleep 90s ..."
    sleep 90
    container_new_status=`docker ps | grep service_runtime_container`
    echo $container_new_status
    if [ "$container_new_status" = "" ]
    then
        echo "container can not automatic restart"
    else
        echo "container can automatic restart"
    fi
fi
