# Copyright (C) 2021 Intel Corporation

cmake_minimum_required(VERSION 3.16)

set(LIBTORCH_URL "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-1.11.0%2Bcpu.zip")
set(LIBPADDLE_URL "https://paddle-inference-lib.bj.bcebos.com/2.3.0/cxx_c/Linux/CPU/gcc5.4_openblas/paddle_inference.tgz")
set(LIBTENSORFLOW_URL "https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-cpu-linux-x86_64-2.8.0.tar.gz")
set(LIBONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v1.11.0/onnxruntime-linux-x64-1.11.0.tgz")

if(NOT DEFINED CACHE_DIR)
    set(CACHE_DIR "${CMAKE_SOURCE_DIR}/.cache")
endif()
if(NOT EXISTS "${CACHE_DIR}")
    file(MAKE_DIRECTORY "${CACHE_DIR}")
elseif(NOT IS_DIRECTORY "${CACHE_DIR}")
    message(FATAL_ERROR "${CACHE_DIR} is not a directory")
endif()

# Download libtorch if not exist
set(LIBTORCH_DIR "${CACHE_DIR}/libtorch")
if (NOT EXISTS "${LIBTORCH_DIR}")
    message("Downloading libtorch...")
    string(REGEX REPLACE  "^.*/" "" LIBTORCH "${LIBTORCH_URL}")
    string(REPLACE  "%2B" "+" LIBTORCH "${LIBTORCH}")
    #execute_process(COMMAND bash -c "t=$\{1##*/};printf '%b' \"$\{t//%/\\\\x}\"" x "${LIBTORCH_URL}" OUTPUT_VARIABLE LIBTORCH)
    file(DOWNLOAD "${LIBTORCH_URL}" "${CACHE_DIR}/${LIBTORCH}" SHOW_PROGRESS)
    execute_process(COMMAND unzip "${CACHE_DIR}/${LIBTORCH}" -d "${CACHE_DIR}")
endif()
set(Torch_DIR "${LIBTORCH_DIR}/share/cmake/Torch")

# Download libtensorflow if not exist
set(LIBTENSORFLOW_DIR "${CACHE_DIR}/libtensorflow")
if (NOT EXISTS "${LIBTENSORFLOW_DIR}")
    message("Downloading libtensorflow...")
    file(MAKE_DIRECTORY "${LIBTENSORFLOW_DIR}")
    execute_process(COMMAND sh -c "curl -L ${LIBTENSORFLOW_URL} | tar -zxf -" WORKING_DIRECTORY "${LIBTENSORFLOW_DIR}")
endif()
set(tensorflow_INSTALL_PREFIX "${LIBTENSORFLOW_DIR}")

# Download onnxruntime if not exist
file(GLOB LIBONNXRUNTIME_DIR "${CACHE_DIR}/onnxruntime-linux-x64-*")
if (NOT LIBONNXRUNTIME_DIR)
    message("Downloading onnxruntime...")
    execute_process(COMMAND sh -c "curl -L ${LIBONNXRUNTIME_URL} | tar -zxf -" WORKING_DIRECTORY "${CACHE_DIR}")
    file(GLOB LIBONNXRUNTIME_DIR "${CACHE_DIR}/onnxruntime-linux-x64-*")
endif()
set(onnxruntime_INSTALL_PREFIX "${LIBONNXRUNTIME_DIR}")

# Download paddle if not exist
set(LIBPADDLE_DIR "${CACHE_DIR}/paddle_inference")
if (NOT EXISTS "${LIBPADDLE_DIR}")
    message("Downloading paddlepaddle...")
    execute_process(COMMAND sh -c "curl -L ${LIBPADDLE_URL} | tar -zxf -" WORKING_DIRECTORY "${CACHE_DIR}")
endif()
set(PADDLE_LIB "${LIBPADDLE_DIR}")

add_subdirectory(runtime_service/src)
