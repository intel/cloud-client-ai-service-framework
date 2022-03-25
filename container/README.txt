Integration
===========
a) dependencies
    OS: Ubuntu 18.04
    OpenVINO: 2021.1
    Docker: > 19.03.0
    packages:
	1) sudo apt install git-lfs libfcgi-dev libcurl4-openssl-dev libssl-dev \
		libgrpc-dev libgrpc++-dev libprotobuf-dev protobuf-compiler \
		protobuf-compiler-grpc python3-dev
	2) sudo pip3 install grpcio==1.30.0 grpcio-tools==1.30.0
	3) pybind11:
		git clone https://github.com/pybind/pybind11
		cd pybind11; git checkout -b tmp f31df73
		mkdir build; cd build; cmake ..; sudo make install

b) build
    cd ./script/integration_workdir
    make base_image
    make openvino_image
    make

Start/Stop service_runtime
==========================
cd ./script/integration_workdir/service_runtime
./service_runtime.sh start|stop
