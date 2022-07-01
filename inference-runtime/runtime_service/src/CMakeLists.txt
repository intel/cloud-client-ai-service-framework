# Copyright (C) 2021 Intel Corporation
#

set (RUNTIME_SERVICE_NAME inferservice)

set (RUNTIME_SERVICE_LIBRARY_HEADERS
    vino_ie_pipe.hpp
    policy.hpp
    cpu_state.hpp
    shared_lib_load.hpp
    shared_lib_pointer.hpp
    iinfer_entity.hpp
    infer_entity.hpp
    irt_inference_engine.hpp
    )

set (RUNTIME_SERVICE_LIBRARY_SOURCES
    vino_ie_pipe.cpp
    policy.cpp
    cpu_state.cpp
    connect_server.cpp
    shared_lib_load.cpp
    iinfer_entity.cpp
    irt_inference_engine.cpp
    )

add_library(${RUNTIME_SERVICE_NAME} SHARED ${RUNTIME_SERVICE_LIBRARY_SOURCES} ${RUNTIME_SERVICE_LIBRARY_HEADERS})

set (CMAKE_CXX_STANDARD 11)

find_package(CURL REQUIRED)
find_package(OpenCV REQUIRED core imgproc imgcodecs)
find_package(Threads REQUIRED)
find_package(pybind11 REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)

set(CCAI_LOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../log)
find_library(CCAI_LOG_LIBRARY ccai_log ${CCAI_LOG_DIR} NO_DEFAULT_PATH)
set(CCAI_LOG_INCLUDE ${CCAI_LOG_DIR})

target_link_libraries(${RUNTIME_SERVICE_NAME} PRIVATE  ${OpenCV_LIBRARIES}
                                                       ${CURL_LIBRARIES}
                                                       ${CCAI_LOG_LIBRARY})

target_include_directories(${RUNTIME_SERVICE_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS}
                                                           ${CURL_INCLUDE_DIRS}
                                                           ${CCAI_LOG_INCLUDE})

add_subdirectory(openvino)
add_subdirectory(onnx)
add_subdirectory(tensorflow)
add_subdirectory(pytorch)
add_subdirectory(paddle)

set(POLICY_DAEMON_NAME policy_daemon)
set(DAEMON_SOURCES daemon_policy.cpp cpu_state.cpp policy.cpp)
set(DAEMON_HEADERS cpu_state.hpp policy.hpp)
add_executable(${POLICY_DAEMON_NAME} ${DAEMON_SOURCES} ${DAEMON_HEADERS})
target_link_libraries(${POLICY_DAEMON_NAME} PRIVATE Threads::Threads)

add_executable(encrypt encrypt.c)
target_link_libraries(encrypt crypto)

#python binding
set (RUNTIME_PYTHON_NAME inferservice_python)
pybind11_add_module(${RUNTIME_PYTHON_NAME} vino_ie_pipe_py.cpp vino_ie_pipe.hpp)
target_link_libraries(${RUNTIME_PYTHON_NAME} PRIVATE ${RUNTIME_SERVICE_NAME}
                                                     ${OpenCV_LIBRARIES})
target_include_directories(${RUNTIME_PYTHON_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS}
                                                          ${Python3_INCLUDE_DIRS})
