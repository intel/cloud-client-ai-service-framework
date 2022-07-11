1: ./runtime_service folder is used to generate runtime service library, which provides the simple IE API to user.
  (1) cd ./runtime_service
  (2) source /opt/intel/openvino/bin/setupvars.sh 
      ./build_runtime_lib.sh
      All the files exported (libs, binary or header files) are put in release folder.
  (3) APIs:
      vino_ie_pipeline_infer_image()  --  processing images
      vino_ie_pipeline_infer_speech() --  processing speech, use intel speech library. 
      vino_ie_video_infer_init()      --  video initialization
      vino_ie_video_infer_frame()     --  processing video frames

2: ./test_runtime_service folder is used to test runtime service library. This is the test code.
  (1) cd ./test_runtime_service
  (2) source /opt/intel/openvino/bin/setupvars.sh 
      ./build_test.sh 
      The test file(test_runtime_service) is ouputed in ~/ie_runtime_build/intel64/Release.
  (3) The test file(test_runtime_service) is also copied to the current folder by script.
      Runing this test file. Notice: you should prepare models files and put them under ./models folder.

      ./test_runtime_service 
      
      Test file will run the following cases:
        -- object-detection-sample-ssd
        -- LPR: security-camera
        -- face detection and facial landmark detections
        -- Intel ASR with intel speech library
        -- Test video API

3: ./release folder is the place to hold exporting header file and library.
      ./release/lib includes all libraries.
      ./release/include includes the header file(APIs) exported by libinferservice.so
      ./release/bin includes policy daemon.
      ./release/policy_setting.cfg is the configuration file.

    ./build_runtime_lib.sh will create this release folder during compiling.

4: building dependency:
   depend on libraries: libcurl and openssl.
   Install the following libs:
   # sudo apt-get install libcurl openssl libcurl4-openssl-dev

   Notice:
   ubuntu16.04, curl headerfiles are in /usr/include/curl/
   ubuntu18.04, curl headerfiles are in /usr/include/x86_64-linux-gnu/curl/

5: python binding
   Currently, runtime inference library provides python interface to python applications.
   The runtime inference library supports python3.6.
   Dependencies: python3-dev pybind11
   #sudo apt-get install python3-dev
   #sudo pip install pybind11

   The python library is inferservice_python.cpython-36m-x86_64-linux-gnu.so.
   It is put under ../release/lib/ .

   python APIs are:
   infer_image()
   infer_speech()

6: install libtorch in /opt/
   wget  https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.11.0%2Bcpu.zip
   unzip libtorch-cxx11-abi-shared-with-deps-1.11.0+cpu.zip
   cp -r /libtorch /opt/

   Must use C++14 to compile libtorch.

7: Download and install Onnx runtime library
   #wget -c https://github.com/microsoft/onnxruntime/releases/download/v1.11.0/onnxruntime-linux-x64-1.11.0.tgz
   #tar -zxvf onnxruntime-linux-x64-1.11.0.tgz
   #sudo cp -rf onnxruntime-linux-x64-1.11.0 /opt/onnxruntime

8: Download and install TensorFlow runtime library
   # wget https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz
   # sudo mkdir /opt/tensorflow
   # sudo tar -C /opt/tensorflow -zxvf libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz
