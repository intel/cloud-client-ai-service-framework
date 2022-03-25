# Copyright (C) 2020 Intel Corporation

cmake_minimum_required(VERSION 3.10)
project(health_monitor_agent)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DDEBUG)
endif()

include(FindProtobuf)
find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
find_package(Threads)

# libs
include(FindPkgConfig)
pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc++>=1.3.2)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf>=3.0.0)
find_package(PythonInterp 3.6 REQUIRED)
find_package(PythonLibs REQUIRED)
if (PythonLibs_FOUND)
  message("ok: " ${PYTHON_LIBRARY})
endif()

macro(add_proto proto_name)
	get_filename_component(proto "${proto_name}.proto" ABSOLUTE)
	get_filename_component(proto_path "${proto}" PATH)
	set(proto_py "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}_pb2.py")
	set(grpc_py "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}_pb2_grpc.py")
	add_custom_command(
		OUTPUT "${proto_py}" "${grpc_py}"
		COMMAND "python3"
		ARGS  -m grpc_tools.protoc
		      --python_out="${CMAKE_CURRENT_BINARY_DIR}"
		      --grpc_python_out="${CMAKE_CURRENT_BINARY_DIR}"
		      -I "${proto_path}" "${proto}"
		DEPENDS "${proto}")

	add_custom_target(${proto_name}
		COMMAND echo "genrate ${proto_name} python files"
		DEPENDS ${proto_py} ${grpc_py})
endmacro()

add_proto(service_runtime_health_monitor)
add_proto(inference_service)

add_custom_target(health_monitor_agent ALL
	COMMAND echo "ALL target"
	DEPENDS service_runtime_health_monitor inference_service)

install(DIRECTORY DESTINATION /opt/health_monitor)
install(PROGRAMS ${CMAKE_SOURCE_DIR}/service_runtime_health_monitor_client.py
	${CMAKE_CURRENT_BINARY_DIR}/inference_service_pb2.py
	${CMAKE_CURRENT_BINARY_DIR}/inference_service_pb2_grpc.py
	${CMAKE_CURRENT_BINARY_DIR}/service_runtime_health_monitor_pb2.py
	${CMAKE_CURRENT_BINARY_DIR}/service_runtime_health_monitor_pb2_grpc.py
	DESTINATION /opt/health_monitor)

install(FILES ${CMAKE_SOURCE_DIR}/config.yml
	DESTINATION /opt/health_monitor)
