# Copyright (C) 2020 Intel Corporation

cmake_minimum_required(VERSION 3.10)
project(service_runtime_health_monitor)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DDEBUG)
endif()

include(FindProtobuf)
find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
find_package(Threads)

# include, cflags
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
add_definitions(-std=c++11 -g -Wall -O2)

# libs
include(FindPkgConfig)
pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc++>=1.3.2)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf>=3.0.0)
find_package(PythonInterp 3.6 REQUIRED)
find_package(PythonLibs REQUIRED)
if (PythonLibs_FOUND)
  message("ok: " ${PYTHON_LIBRARY})
endif()

set(proto_name "service_runtime_health_monitor")
get_filename_component(proto "${proto_name}.proto" ABSOLUTE)
get_filename_component(proto_path "${proto}" PATH)
protobuf_generate_cpp(proto_src proto_hdr ${proto})

set(grpc_src "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.cc")
set(grpc_hdr "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h")
add_custom_command(
	OUTPUT "${grpc_src}" "${grpc_hdr}"
	"${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h"
	COMMAND protobuf::protoc
	ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}" -I "${proto_path}"
	--plugin=protoc-gen-grpc="${gRPC_CPP_PLUGIN_EXECUTABLE}" "${proto}"
	DEPENDS "${proto}")
# generate pythone proto/grpc code
set(proto_py "${CMAKE_CURRENT_BINARY_DIR}/${name}_pb2.py")
set(grpc_py "${CMAKE_CURRENT_BINARY_DIR}/${name}_pb2_grpc.py")
add_custom_command(
	OUTPUT "${proto_py}" "${grpc_py}"
	COMMAND "python3"
	ARGS  -m grpc_tools.protoc --python_out="${CMAKE_CURRENT_BINARY_DIR}"
	--grpc_python_out="${CMAKE_CURRENT_BINARY_DIR}"
	-I "${proto_path}" "${proto}"
	DEPENDS "${proto}")

add_executable(service_runtime_health_monitor
	service_runtime_health_monitor.cc
	service_runtime_health_monitor.h
	${proto_src} ${proto_hdr} ${grpc_src} ${grpc_hdr}
	${proto_py} ${grpc_py})

target_link_libraries(service_runtime_health_monitor
	${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

install(DIRECTORY DESTINATION /opt/intel/service_runtime)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/service_runtime_health_monitor
	DESTINATION /opt/intel/service_runtime/)
