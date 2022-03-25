# Intel Cloud-Client AI Service Framework

## How to build Cloud-Client AI Service Framework
Make sure your Build Host meets the following requirements:
- Ubuntu 20.04.
- OpenVINO 2021.4.

You must install essential host packages on your build host.
The following command installs the host packages:
```
sudo ln -sf /opt/intel/openvino_2021 /opt/intel/openvino

sudo apt-get update
sudo apt-get install docker.io build-essential wget cmake python3-pip unzip \
	    libfcgi-dev libcurl4-openssl-dev libssl-dev libpam0g-dev \
	    libgrpc-dev libgrpc++-dev libprotobuf-dev protobuf-compiler  \
	    protobuf-compiler-grpc \
	    python3-dev \
	    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
	    gstreamer1.0-tools gstreamer1.0-x \
	    libpci-dev pciutils  libjson-c-dev libsqlite3-dev

sudo python3 -m pip install pytest grpcio-tools kconfiglib torch torchvision \
	     paddlepaddle paddle2onnx
sudo python3 -m pip install -r /opt/intel/openvino/deployment_tools/open_model_zoo/tools/downloader/requirements.in
sudo python3 -m pip install -r /opt/intel/openvino/deployment_tools/model_optimizer/requirements.txt

git clone https://github.com/pybind/pybind11
cd pybind11 && git checkout -b tmp f31df73
mkdir build && cd build && cmake ..
sudo make -j$(nproc --all) install

wget  https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.7.1%2Bcpu.zip
unzip libtorch-cxx11-abi-shared-with-deps-1.7.1+cpu.zip
sudo cp -r libtorch/ /opt/ && rm -rf libtorch*

wget -c https://github.com/microsoft/onnxruntime/releases/download/v1.7.0/onnxruntime-linux-x64-1.7.0.tgz
tar -zxvf onnxruntime-linux-x64-1.7.0.tgz
sudo mv onnxruntime-linux-x64-1.7.0 /opt/onnxruntime

wget https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-cpu-linux-x86_64-2.5.0.tar.gz
sudo mkdir -p /opt/tensorflow
sudo tar -C /opt/tensorflow -zxvf libtensorflow-cpu-linux-x86_64-2.5.0.tar.gz
rm -f libtensorflow-cpu-linux-x86_64-2.5.0.tar.gz

```
build Cloud-Client AI Service Framework with the following command
```
cd container/script/integration_workdir
make defconfig
make base_image
make inference_engine_image
make image
```

download models (If you have already executed `get_models.sh`, all models should be cached in the folder, `get_models_cache`, you can execute `get_models.sh -i` to just install the models into the correct directory to save time.)

```
cd container/script/integration_workdir
../get_models.sh
```

launch Cloud-Client AI Service Framework

```
cd container/script/integration_workdir/service_runtime/
./service_runtime.sh start
```
