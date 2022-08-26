# Intel Cloud-Client AI Service Framework

[Documentation](https://intel.github.io/cloud-client-ai-service-framework/)

The latest release is [v1.3](https://github.com/intel/cloud-client-ai-service-framework/tree/v1.3)

## How to build Cloud-Client AI Service Framework
Make sure your Build Host meets the following requirements:
- Ubuntu 20.04.

You must install essential host packages on your build host.
The following command installs the host packages:
```
sudo apt update
sudo apt install git build-essential docker.io wget cmake python3-pip unzip \
        libfcgi-dev libcurl4-openssl-dev libssl-dev libpam0g-dev \
        libgrpc-dev libgrpc++-dev libprotobuf-dev protobuf-compiler protobuf-compiler-grpc \
        python3-dev \
        libpci-dev pciutils  libjson-c-dev libsqlite3-dev \
        curl gpg-agent software-properties-common

curl https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | sudo apt-key add -
echo "deb https://apt.repos.intel.com/openvino/2022 `. /etc/os-release && echo ${UBUNTU_CODENAME}` main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2022.list

sudo apt update
sudo apt install openvino-libraries-dev-2022.1.0 openvino-samples-2022.1.0 intel-dlstreamer-dev
sudo /opt/intel/openvino_2022/install_dependencies/install_NEO_OCL_driver.sh
sudo /opt/intel/dlstreamer/install_dependencies/install_media_driver.sh

sudo python3 -m pip install --upgrade pip
sudo python3 -m pip install pytest grpcio-tools kconfiglib torch torchvision paddlepaddle paddle2onnx
sudo python3 -m pip install openvino-dev[caffe,kaldi,mxnet,onnx,pytorch,tensorflow,tensorflow2]==2022.1.0

git clone https://github.com/pybind/pybind11
cd pybind11 && git checkout -b tmp f31df73
mkdir build && cd build && cmake ..
sudo make -j$(nproc --all) install

wget  https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.11.0%2Bcpu.zip
unzip libtorch-cxx11-abi-shared-with-deps-1.11.0+cpu.zip
sudo cp -r libtorch/ /opt/ && rm -rf libtorch*

wget -c https://github.com/microsoft/onnxruntime/releases/download/v1.7.0/onnxruntime-linux-x64-1.7.0.tgz
tar -zxvf onnxruntime-linux-x64-1.7.0.tgz
sudo mv onnxruntime-linux-x64-1.7.0 /opt/onnxruntime

wget https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz
sudo mkdir -p /opt/tensorflow
sudo tar -C /opt/tensorflow -zxvf libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz
rm -f libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz

wget  https://paddle-inference-lib.bj.bcebos.com/2.3.0/cxx_c/Linux/CPU/gcc5.4_openblas/paddle_inference.tgz
tar -zxvf paddle_inference.tgz
sudo mv paddle_inference/ /opt/
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
