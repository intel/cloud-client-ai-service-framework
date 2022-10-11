# Copyright (C) 2020 Intel Corporation

cmake_minimum_required(VERSION 3.16)
project(service_runtime_grpc_service)
include(GNUInstallDirs)

find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf>=3.0.0)
pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc++>=1.3.2)
find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
find_package(Log REQUIRED)
find_package(RuntimeService REQUIRED)
if(NOT DEFINED OpenCV_FOUND)
  find_package(OpenCV REQUIRED core imgproc imgcodecs)
endif()

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

add_executable(grpc_inference_service
               grpc_inference_service.cc
               asr.cc asr.h
               ocr.cc ocr.h
               ../fcgi/utils/cpp/fcgi_utils.cpp
               ../fcgi/utils/cpp/fcgi_utils.h)
target_compile_definitions(grpc_inference_service PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_include_directories(grpc_inference_service PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(grpc_inference_service PRIVATE CCAI::RuntimeService
                                                     CCAI::Log
                                                     ${OpenCV_LIBRARIES}
                                                     PkgConfig::GRPC
                                                     ${Protobuf_LIBRARY}
                                                     -lpam)
add_proto(inference_service)
add_dependencies(grpc_inference_service CCAI::RuntimeService)
install(TARGETS grpc_inference_service RUNTIME
        DESTINATION "${CMAKE_INSTALL_SBINDIR}")

macro(install_symlink filepath sympath)
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E
  create_symlink ${filepath} $ENV{DESTDIR}/${sympath})")
  install(CODE "message(\"-- Created symlink: ${sympath} -> ${filepath}\")")
endmacro(install_symlink)

macro(install_service_launch_script service)
install(DIRECTORY DESTINATION "/etc/sv/${service}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/config/runit/${service}/run"
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
        GROUP_READ GROUP_EXECUTE WORLD_EXECUTE WORLD_READ
        DESTINATION "/etc/sv/${service}")
# disable grpc services by default
install_symlink(/etc/sv/${service} /etc/runit/runsvdir/default/${service})
endmacro(install_service_launch_script)

function(install_service name port)
  set(src "${CMAKE_CURRENT_SOURCE_DIR}/config/runit/grpc_inference_service/run")
  install_get_abspath(dir "${CMAKE_INSTALL_SYSCONFDIR}/sv/${name}")
  install(DIRECTORY DESTINATION "${dir}")
  install(CODE "execute_process(COMMAND sh -c \"sed 's/50001/${port}/' '${src}' > run\"
                                COMMAND chmod 0755 run
                                WORKING_DIRECTORY \"${dir}\")")
  install_link("/tmp/${name}/supervise" "${dir}/" SYMBOLIC)
  install_link("../../../sv/${name}" "${CMAKE_INSTALL_SYSCONFDIR}/runit/runsvdir/default/" SYMBOLIC)
endfunction()

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

# add service
install_service(grpc_inference_service 50001)
if ("$ENV{CONFIG_GRPC_SAMPLE}" STREQUAL "y")
  install_service(grpc_inference_service_classification 50002)
  install_service(grpc_inference_service_facedetection 50003)
  install_service(grpc_inference_service_faciallandmark 50004)
  install_service(grpc_inference_service_ocr 50005)
endif()

# install nghttpx config file
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/nghttpx.conf"
        DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/nghttpx")

# install nghttpx service config file
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/runit/nghttpx/run"
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
        GROUP_READ GROUP_EXECUTE WORLD_EXECUTE WORLD_READ
        DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/sv/nghttpx")

# nghttpx
if ("$ENV{CONFIG_GRPC_SAMPLE}" STREQUAL "y")
  install_link("../../../sv/nghttpx" "${CMAKE_INSTALL_SYSCONFDIR}/runit/runsvdir/default/" SYMBOLIC)
  install_link("/tmp/nghttpx/supervise" "${CMAKE_INSTALL_SYSCONFDIR}/sv/nghttpx/" SYMBOLIC)
endif()
