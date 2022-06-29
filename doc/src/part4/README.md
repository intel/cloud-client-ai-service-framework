# 4. How to setup development environment

For convenient development, we provide a development container which includes all dependencies for developing services for CCAI. This is an option for convenience, you still can develop CCAI service in your working environment like Ubuntu 20.04 etc.

## 4.1 Download and run development docker image

You can download a pre-built for-development docker image and a launcher script
directly for development:

For example, download package to home/pub/images/service_runtime_devel.tar.gz .

And download initial script to /home/pub/images/service_runtime_devel.sh .

Load image:

    $> gzip -cd service_runtime_devel.tar.gz | docker load

Launch the development docker image:

    $> service_runtime_devel.sh

Now you can enter into the development container environment for CCAI service
development. And if you would like to change CCAI framework core itself and you
also have CCAI framework source code access, then you also can copy source code
to an accessible fold (like your \$HOME) and have CCAI core development within
the container.

## 4.2 Enter development container environment {#4.2}

After executing 'service_runtime_devel.sh', a container named
'service_runtime_devel_container_$USER' will be run. To enter the container,
you can execute:

    $> docker exec -it service_runtime_devel_container_$USER /bin/bash

The host \$HOME folder will be mounted to the container, so in the container,
you can directly access the files in the host \$HOME and edit/compile/verify
your changes in CCAI services/core. For example, you can set up a link from the
container folder to your home directory and after changes, just restart
container image to verify your modification.

## 4.3 Setup development environment directly in your machine {#4.3}

**Note: all components versions mentioned below in this document are for
example, they will change/update according to new features/new releases in
following days, so please replace the specific version with those exact
versions/releases depending on what you get.**

The steps above are convenient for CCAI related development. But in case, where
you cannot use the pre-built devel container image, then you still can set up
the environment in your machines directly. Basically, you can use any
development box for this, and we recommend having Ubuntu 20.04 as host OSes
because we only verified those processes against it.

The basic dependencies could be found from section 3.2.1 above. Besides, you
need to install some additional packages for having a simple CCAI host
environment (non container).

Please follow instructions below:

    $>wget https://apt.repos.intel.com/openvino/2021/GPG-PUB-KEY-INTEL-OPENVINO-2021
    
    $>sudo apt-key add GPG-PUB-KEY-INTEL-OPENVINO-2021
    
    $> echo "deb https://apt.repos.intel.com/openvino/2021 all main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2021.list
    
    $>sudo apt-get update
    
    $>sudo apt-get install \
    
    lighttpd lighttpd-mod-authn-pam libfcgi nghttp2-proxy libgrpc++1 gnupg \
    
    python3 python3-pip python3-setuptools libnuma1 ocl-icd-libopencl1 \
    
    libgtk-3-0 gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
    
    python3-gi libxv1 \
    
    intel-openvino-runtime-ubuntu20-2021.3.394 \
    
    intel-openvino-gstreamer-rt-ubuntu-focal-2021.3.394 \
    
    intel-openvino-gva-rt-ubuntu-focal-2021.3.394 \
    
    unzip intel-gpu-tools libcurl4 python3-yaml libpci3 \
    
    $>sudo pip3 install --only-binary :all: flup \
    
    numpy opencv-python cython nibabel scikit-learn scipy tqdm \
    
    requests grpcio protobuf
    
    $>sudo pip3 install llvmlite==0.31.0 numba==0.48 librosa==0.6.3 \
    
    pytest grpcio-tools
    
    $>wget https://github.com/intel/compute-runtime/releases/download/20.41.18123/intel-gmmlib_20.3.1_amd64.deb
    
    $>wget https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.5186/intel-igc-core_1.0.5186_amd64.deb
    
    $>wget https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.5186/intel-igc-opencl_1.0.5186_amd64.deb
    
    $>wget https://github.com/intel/compute-runtime/releases/download/20.41.18123/intel-opencl_20.41.18123_amd64.deb
    
    $>wget https://github.com/intel/compute-runtime/releases/download/20.41.18123/intel-ocloc_20.41.18123_amd64.deb
    
    $>wget https://github.com/intel/compute-runtime/releases/download/20.41.18123/intel-level-zero-gpu_1.0.18123_amd64.deb
    
    $>sudo dpkg -i *.deb
    
    $>wget https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.11.0%2Bcpu.zip
    
    $>unzip libtorch-cxx11-abi-shared-with-deps-1.11.0+cpu.zip
    
    $>sudo mkdir -p /opt/fcgi/cgi-bin
    
    $>sudo cp libtorch/lib/libc10.so /opt/fcgi/cgi-bin/
    
    $>sudo cp /libtorch/lib/libgomp*.so.1 /opt/fcgi/cgi-bin/
    
    $>sudo cp /libtorch/lib/libtorch_cpu.so /opt/fcgi/cgi-bin/
    
    $>sudo cp /libtorch/lib/libtorch.so /opt/fcgi/cgi-bin/
    
    $>sudo cp -r libtorch/ /opt/
    
    $>wget https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz
    
    $>sudo mkdir /opt/tensorflow
    
    $>sudo tar -C /opt/tensorflow -zxvf libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz
    
    $>sudo apt-get install intel-openvino-dev-ubuntu20-2021.1.1 \
    
    build-essential cmake git git-lfs libfcgi-dev libcurl4-openssl-dev \
    
    libssl-dev libpam0g-dev libgrpc-dev libgrpc++-dev \
    
    libprotobuf-dev protobuf-compiler protobuf-compiler-grpc python3-dev \
    
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libpci-dev \
    
    docker.io
    
    $>git clone https://github.com/pybind/pybind11>
    
    $>cd pybind11 && git checkout -b tmp f31df73
    
    $>mkdir build && cd build && cmake .. && make -j\$(nproc --all) install

The development process has nothing special but please noted:

1. If you are developing an application using CCAI existing services, you'd
    better launch a CCAI container for testing. You can refer to section 3.2 to
    get/install CCAI container and host packages.

2. If you are developing CCAI services which will be provided to applications
    to use, you can develop and verify them in host and if these services work
    as expected, you can then deploy your completed services into the CCAI
    container and then launch the CCAI container for verification. The
    development and deployment of services can be found in chapter 5 and 6.

3. If you are developing the CCAI core itself, it is similar to step 2 above
    but what you need to change are core libraries and the container itself, you
    also can find useful information from chapter 5 and 6.

## 4.4 Setup the Pulseaudio service {#4.4}

If you want to enable sound e2e cases, such as TTS or live asr cases, you need to enable the pulseaudio service on the host side.

(1) On the host PC, install the pulseaudio package if this package hasn't been installed.

For example:

    #sudo apt-get install pulseaudio

(2) Enable the TCP protocol of the pulseaudio.

Edit the configuration file. for example:

    #sudo vim /etc/pulse/default.pa

Find out the following tcp configuration:

    #load-module module-native-protocol-tcp

Uncomment the tcp configuration(remove "    #"), and add authentication:

    load-module module-native-protocol-tcp auth-anonymous=1

Save and quit the configuration file.

(3) Restart the pulseaudio service. For example:

    #sudo systemctl restart pulseaudio

Or kill the pulseaudio service directly:

    #sudo kill -9 xxxx(pulseaudio thread number)
