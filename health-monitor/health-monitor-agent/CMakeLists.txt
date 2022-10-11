# Copyright (C) 2020 Intel Corporation

cmake_minimum_required(VERSION 3.16)
project(health_monitor_agent)

#   RESULT_VARIABLE EXIT_CODE
find_package(Python3 REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import grpc_tools.protoc"
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_QUIET
)


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

#install(DIRECTORY DESTINATION /opt/health_monitor)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/service_runtime_health_monitor_client.py
	${CMAKE_CURRENT_BINARY_DIR}/inference_service_pb2.py
	${CMAKE_CURRENT_BINARY_DIR}/inference_service_pb2_grpc.py
	${CMAKE_CURRENT_BINARY_DIR}/service_runtime_health_monitor_pb2.py
	${CMAKE_CURRENT_BINARY_DIR}/service_runtime_health_monitor_pb2_grpc.py
	DESTINATION opt/health_monitor)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.yml
	DESTINATION opt/health_monitor)
