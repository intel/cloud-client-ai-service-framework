# 8. How to integrate new AI services with CCAI framework

Once you have new services, to make it work to be able to accept requests from outside of the CCAI container and give back the result of one specific AI task, you will have to deploy those services in the CCAI container.

## 8.1 Where to put those services file to {#8.1}

Please extract the CCAI release tar file, saying 'ccaisf_release_xx-xxx.tar.gz' and copy your files and directories organized in a runtime hierarchy to the folder 'docker/app_rootfs'.

A fast CGI service example as:
ccaisf_release_xx-xxx/docker/app_rootfs

    |-- etc
    |  |-- lighttpd
    |  |  |-- conf-available
    |  |  |  |-- 16-classification.conf
    |  |  |-- conf-enabled
    |  |  |  |-- 16-classification.conf -> ../conf-available/16-classification.conf
    |-- opt
    |  |-- fcgi
    |  |  |__ cgi-bin
    |  |  |  |-- fcgi_classfication

A gRPC service example as:

ccaisf_release_xx-xxx/docker/app_rootfs

    |-- etc
    |  |__ sv
    |     |-- grpc_inference_service_speech_1
    |     |  |-- run
    |     |  |--  supervise -> /tmp/grpc_inference_service_speech_1/supervise
    |  |-- runit
    |  |  |-- runsvdir
    |  |    |-- default
    |  |      |-- grpc_inference_service_speech_1 ->/etc/sv/grpc_inference_service_speech_1
    |-- usr
      |-- sbin
        |-- grpc_inference_service

## 8.2 Where to put related Neural Network Models file {#8.2}

Models will be installed to the folder, '/opt/intel/service_runtime/models' on host, CCAI will map the folder to container's '/opt/fcgi/cgi-bin/models', so you can write your debian package configuration files and build your deb package to install your models, otherwise you can also put your models to CCAI release folder to use CCAI's helper script to build your deb package:

1. Put models to 'ccaisf_release_xx-xxx/package/models'.

2. Modify 'ccaisf_release_xx-xxx/package/models/debian/control' to add your package.

3. Add a 'service-runtime-models-xxx.install' file to
    'ccaisf_release_xx-xxx/package/models/debian' to install your models.

## 8.3 How to enable services via API gateway {#8.3}

As we had described in chapter 6.3, for fast CGI services, you need to
add/change conf files and put those conf files under specific folders so that API gateway will recognize your services and launch them according to the configuration file description.

For gRPC services, if it is a brand new services, you need to do the following steps to enable the service:

1.Create a folder under docker/app_rootfs/etc/sv/, example:

    $> mkdir docker/app_rootfs/etc/sv/your_grpc_service

2.Write a script named 'run' to launch your service, and put the script to 'docker/app_rootfs/etc/sv/your_grpc_service', example:

    $>cat > docker/app_rootfs/etc/sv/your_grpc_service/run < EOF
    #!/bin/bash
    exec /usr/sbin/your_service
    EOF

3.Create a link to manage the service at runtime.

    $>ln -sf /tmp/your_grpc_service/supervise docker/app_rootfs/etc/sv/your_grpc_service/

4.Create a link to enable your gRPC service

    $>ln -sf /etc/sv/your_grpc_service docker/app_rootfs/etc/runit/runsvdir/default/

## 8.4 How to generate new container image {#8.4}

Please refer to Chapter 5, execute 'release_build.sh 'to generate a new
container image or deb package for models.

    $> cd ccaisf_release_xx-xxx
    $> ./release_build.sh image
    $> ./release_build.sh models_package
