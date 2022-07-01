# Copyright (C) 2020 Intel Corporation

cmake_minimum_required(VERSION 3.12)
project(service_runtime_grpc_service)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DDEBUG)
endif()

include(FindProtobuf)
find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)

# include, cflags
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
add_compile_options(-Wall -fPIE -fstack-protector-all -D_FORTIFY_SOURCE=2)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,now -Wl,-z,relro")

# libs
include(FindPkgConfig)
pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc++>=1.3.2)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf>=3.0.0)

find_package(OpenCV REQUIRED core imgproc imgcodecs)

set(CCAI_LOG_DIR ${CMAKE_SOURCE_DIR}/../../log)
find_library(CCAI_LOG_LIBRARY
  ccai_log
  ${CCAI_LOG_DIR}
  NO_DEFAULT_PATH)
set(CCAI_LOG_INCLUDE ${CCAI_LOG_DIR})

# add runtime library support
set(INFER_SERVICE_LIB ${CMAKE_SOURCE_DIR}/../../inference-runtime/release/lib)
find_library(RUNTIME_LIBRARY_TARGET_NAME
  inferservice
  ${INFER_SERVICE_LIB}
  NO_DEFAULT_PATH)
set (Runtime_LIBRARIES ${RUNTIME_LIBRARY_TARGET_NAME})

macro(add_proto name)
  get_filename_component(proto "${name}.proto" ABSOLUTE)
  get_filename_component(proto_path "${proto}" PATH)
  protobuf_generate_cpp(proto_src proto_hdr ${proto})

  set(grpc_src "${CMAKE_CURRENT_BINARY_DIR}/${name}.grpc.pb.cc")
  set(grpc_hdr "${CMAKE_CURRENT_BINARY_DIR}/${name}.grpc.pb.h")
  add_custom_command(
    OUTPUT "${grpc_src}" "${grpc_hdr}"
           "${CMAKE_CURRENT_BINARY_DIR}/${name}.grpc.pb.h"
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
  target_sources(grpc_inference_service PUBLIC
    ${proto_src} ${proto_hdr} ${grpc_src} ${grpc_hdr} ${proto_py} ${grpc_py})
endmacro()

# generate sources .pb/.grpc.pb
macro(add_service service_name)
  # service
  add_executable(${service_name} ${service_name}.cc ${service_name}.h)

  target_include_directories(${service_name} PRIVATE
                             ${CMAKE_SOURCE_DIR}/../../inference-runtime/release/include
                             ${CCAI_LOG_INCLUDE})
  target_link_libraries(${service_name}
    ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES}
    ${OpenCV_LIBRARIES}
    ${Runtime_LIBRARIES} ${CCAI_LOG_LIBRARY}
    -lpam)
  install(TARGETS ${service_name} DESTINATION /usr/sbin)
endmacro()

add_service(grpc_inference_service)
target_sources(grpc_inference_service PUBLIC ocr.cc ocr.h)
target_sources(grpc_inference_service PUBLIC asr.cc asr.h)
target_sources(grpc_inference_service PUBLIC ../fcgi/utils/cpp/fcgi_utils.cpp ../fcgi/utils/cpp/fcgi_utils.h)
add_proto(inference_service)

macro(install_symlink filepath sympath)
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E
  create_symlink ${filepath} $ENV{DESTDIR}/${sympath})")
  install(CODE "message(\"-- Created symlink: ${sympath} -> ${filepath}\")")
endmacro(install_symlink)

macro(install_service_launch_script service)
install(DIRECTORY DESTINATION /etc/sv/${service})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config/runit/${service}/run
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
    GROUP_READ GROUP_EXECUTE WORLD_EXECUTE WORLD_READ
  DESTINATION /etc/sv/${service})
# disable grpc services by default
install_symlink(/etc/sv/${service} /etc/runit/runsvdir/default/${service})
endmacro(install_service_launch_script)

macro(add_service_config service port)
set(template_file "${CMAKE_SOURCE_DIR}/config/runit/grpc_inference_service/run")
set(config_dir "${CMAKE_CURRENT_BINARY_DIR}/config/runit/${service}")
set(config_file "${config_dir}/run")
add_custom_command(OUTPUT ${config_file}
  COMMAND mkdir ARGS -p ${config_dir}
  COMMAND cp ARGS -a ${template_file} ${config_dir}
  COMMAND sed ARGS -i "s/50001/${port}/" ${config_file}
  DEPENDS ${template_file})
add_custom_target(${service}_config DEPENDS ${config_file})
add_dependencies(grpc_inference_service ${service}_config)
install_service_launch_script(${service})
endmacro()

# install runit service default dir
install(DIRECTORY DESTINATION /etc/runit/runsvdir/default)

# add service
add_service_config(grpc_inference_service 50001)

if ("$ENV{CONFIG_GRPC_SAMPLE}" STREQUAL "y")
	add_service_config(grpc_inference_service_classification 50002)
	add_service_config(grpc_inference_service_facedetection 50003)
	add_service_config(grpc_inference_service_faciallandmark 50004)
	add_service_config(grpc_inference_service_ocr 50005)
endif()

# install nghttpx config file
install(FILES ${CMAKE_SOURCE_DIR}/config/nghttpx.conf DESTINATION /etc/nghttpx)

# install nghttpx service config file
install(DIRECTORY DESTINATION /etc/sv/nghttpx)
install(FILES ${CMAKE_SOURCE_DIR}/config/runit/nghttpx/run
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
    GROUP_READ GROUP_EXECUTE WORLD_EXECUTE WORLD_READ
  DESTINATION /etc/sv/nghttpx)

# nghttpx
if ("$ENV{CONFIG_GRPC_SAMPLE}" STREQUAL "y")
	install_symlink(/etc/sv/nghttpx /etc/runit/runsvdir/default/nghttpx)
endif()

