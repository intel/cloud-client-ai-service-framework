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

find_library(CURL_LIBRARY_TARGET_NAME
    curl libcurl.so.4
    PATHS "/usr/lib/x86_64-linux-gnu/" "/usr/local/lib/"
    NO_DEFAULT_PATH)
message("curl ok: " ${CURL_LIBRARY_TARGET_NAME})
set (Curl_LIBRARIES ${CURL_LIBRARY_TARGET_NAME})
set (Curl_INCLUDES "/usr/include/x86_64-linux-gnu/")

# Find OpenCV components if exist
find_package(OpenCV QUIET)
if(NOT OpenCV_FOUND)
    message(WARNING "OPENCV is disabled or not found, " ${IE_SAMPLE_NAME} " skipped")
    return()
else()
    add_definitions(-DUSE_OPENCV)
endif()

find_package(Threads REQUIRED)
if(Threads_FOUND)
    message(WARNING "Threads lib found")
endif()

# Find pybind header files
find_package(pybind11 REQUIRED)
if (NOT pybind11_FOUND)
    message(WARNING "pybind11 is not found, " ${IE_SAMPLE_NAME} " skipped")
    return()
else()
    message("pybind11 is found")
endif()

set(CCAI_LOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../log)
find_library(CCAI_LOG_LIBRARY ccai_log ${CCAI_LOG_DIR} NO_DEFAULT_PATH)
set(CCAI_LOG_INCLUDE ${CCAI_LOG_DIR})

target_link_libraries(${RUNTIME_SERVICE_NAME} PRIVATE  ${OpenCV_LIBRARIES}
                                                       ${Curl_LIBRARIES}
                                                       ${IMMEDIATE_BINDING_FLAG}
                                                       ${CCAI_LOG_LIBRARY})

target_compile_definitions(${RUNTIME_SERVICE_NAME} PRIVATE RUNTIME_SERVICE_LIBRARY_EXPORT)

target_include_directories(${RUNTIME_SERVICE_NAME} PUBLIC include)
target_include_directories(${RUNTIME_SERVICE_NAME} PRIVATE ${Curl_INCLUDES})
target_include_directories(${RUNTIME_SERVICE_NAME} PRIVATE src)
target_include_directories(${RUNTIME_SERVICE_NAME} PRIVATE /usr/include/python3.8)
target_include_directories(${RUNTIME_SERVICE_NAME} PRIVATE ${CCAI_LOG_INCLUDE})

add_subdirectory(openvino)
add_subdirectory(onnx)
add_subdirectory(tensorflow)
add_subdirectory(pytorch)

set(POLICY_DAEMON_NAME policy_daemon)
set(DAEMON_SOURCES daemon_policy.cpp cpu_state.cpp policy.cpp)
set(DAEMON_HEADERS cpu_state.hpp policy.hpp)
add_executable(${POLICY_DAEMON_NAME} ${DAEMON_SOURCES} ${DAEMON_HEADERS})
target_link_libraries(${POLICY_DAEMON_NAME} PRIVATE Threads::Threads)

add_executable(encrypt encrypt.c)
target_link_libraries(encrypt crypto)

#python binding
set (CMAKE_CXX_STANDARD 11)
find_package(PythonInterp 3.8 REQUIRED)

set (RUNTIME_PYTHON_NAME inferservice_python)
pybind11_add_module(${RUNTIME_PYTHON_NAME} ${RUNTIME_SERVICE_LIBRARY_SOURCES} ${RUNTIME_SERVICE_LIBRARY_HEADERS})
target_link_libraries(${RUNTIME_PYTHON_NAME} PRIVATE ${OpenCV_LIBRARIES}
                                                     ${Curl_LIBRARIES}
                                                     ${IMMEDIATE_BINDING_FLAG}
                                                     ${CCAI_LOG_LIBRARY})

target_include_directories(${RUNTIME_PYTHON_NAME} PUBLIC include)
target_include_directories(${RUNTIME_PYTHON_NAME} PRIVATE ${Curl_INCLUDES})
target_include_directories(${RUNTIME_PYTHON_NAME} PRIVATE src)
target_include_directories(${RUNTIME_PYTHON_NAME} PRIVATE /usr/include/python3.8)
target_include_directories(${RUNTIME_PYTHON_NAME} PRIVATE ${CCAI_LOG_INCLUDE})
