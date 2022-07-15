# 3. Integrate and use CCAI runtime environment

## 3.1 How to install the pre-built runtime (if have) and verify it quickly

### 3.1.1 Prepare

**Note: all components versions mentioned below in this document are for
example, they will change/update according to new features/new releases in
following days, so please replace the specific version with those exact
versions/releases depending on what you get.**

Pre-condition to meet:

System requirements :

1\. Install ubuntu 20.04 release on your host

Execute (if need)

    $>sudo apt update
    
    $>sudo apt install docker.io libgrpc++1

2\. Kernel Config

To support docker, Linux kernel must be configured and compiled with the
following required configuration options:

    CONFIG_CGROUP_DEVICE=y
    
    CONFIG_BRIDGE=y

### 3.1.2 Proxy setting

If you are behind a firewall, you may need to set a proxy correctly for pulling
pre-built docker image of CCAI.Please execute the following commands to check
the setting:

    $>env | grep proxy
    
    no_proxy=localhost
    
    https_proxy=http://your-proxy-server:your-proxy-port
    
    http_proxy=http://your-proxy-server:your-proxy-port

If you configure docker using proxy, please add 'nvbox.bj.intel.com' to 'no_proxy'.

### 3.1.3 Container image preparation

If you want to have a quick try instead of building your own framework image
system, you can pull existing docker image directly. (Otherwise, please build
your own docker image as described at following chapters) in the latter part of
this document before you install host packages and run any testing.):

    $>docker pull your-docker-server:port/your CCAI image:latest

### 3.1.4 Download and install service-framework packages/test cases/docker files in host

After downloaded CCAI package:

    $>sudo dpkg -i service_runtime_debs/*.deb

### 3.1.5 Start/Stop service-framework

By default service-framework will be started automatically after the
installation.You can manually stop / start service-framework by following the
instructions:

Stop:

    $>sudo systemctl stop service-runtime-health-monitor.service
    
    $>sudo systemctl stop service-runtime.service

Start:

    $>sudo systemctl start service-runtime-health-monitor.service
    
    $>sudo systemctl start service-runtime.service

Now, the service container will use ports 8080 and 8081 for http services and
gRPC services respectively, if you'd like to change the ports to fit your
requirements, you can change the default ports setting in file:
    /lib/systemd/system/service-runtime.service.

Open this file, and find this line:

ExecStart=/opt/intel/service_runtime/service_runtime.sh start --port-rest-api
8080 --port-grpc 8081

Then change the ports setting according to your needs.

## 3.2 Verify CCAI functions with samples or test cases

You can execute 'docker ps | grep service_runtime_container to check if the
CCAI container is running.

    $> docker ps | grep service_runtime_container

c9372e841a00 574dee124467 "/start.sh" 16 minutes ago Up 16 minutes
127.0.0.1:8080-8081->8080-8081/tcp, 127.0.0.1:50006-50007->50006-50007/tcp
service_runtime_container

You can execute 'docker exec -it service_runtime_container ps -ef to view all processes running in the CCAI container

    $> docker exec -it service_runtime_container ps -ef

UID PID PPID C STIME TTY TIME CMD

www-data 1 0 0 02:33 pts/0 00:00:00 /sbin/docker-init -- /start.

www-data 6 1 0 02:33 pts/0 00:00:00 /bin/bash /start.sh

 ...

If you want to do more tests, please refer to [Chapter 12](#_sc2siv67h884).
