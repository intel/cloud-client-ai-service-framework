# Copyright (C) 2020 Intel Corporation

cmake_minimum_required(VERSION 3.16)
project(Gateway)
include(GNUInstallDirs)

find_package(Log REQUIRED)
find_package(RuntimeService REQUIRED)
if(NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV REQUIRED core imgproc imgcodecs)
endif()

macro(add_fcgi_binary name)
    include_directories(${PROJECT_SOURCE_DIR}/utils/cpp)
    add_executable(${name} ${PROJECT_SOURCE_DIR}/utils/cpp/fcgi_utils.cpp ${name}.cpp)
    target_link_libraries(${name} PRIVATE CCAI::RuntimeService
                                          CCAI::Log
                                          ${OpenCV_LIBRARIES}
                                          fcgi)
    add_dependencies(${name} CCAI::RuntimeService)
    install(TARGETS ${name} DESTINATION opt/fcgi/cgi-bin)
    string(REPLACE "fcgi_" "" config ${name})
    install(FILES 16-${config}.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/lighttpd/conf-available)
    install_link("../conf-available/16-${config}.conf" "${CMAKE_INSTALL_SYSCONFDIR}/lighttpd/conf-enabled/" SYMBOLIC)
endmacro()

macro(add_cap_api_fcgi_binary name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE fcgi)
    install(TARGETS ${name} DESTINATION opt/fcgi/cgi-bin)
    string(REPLACE "fcgi_" "" config ${name})
    install(FILES 16-${config}.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/lighttpd/conf-available)
    install_link("../conf-available/16-${config}.conf" "${CMAKE_INSTALL_SYSCONFDIR}/lighttpd/conf-enabled/" SYMBOLIC)
endmacro()

set(proto_name "service_runtime_health_monitor")
get_filename_component(proto "../../health-monitor/health-monitor/${proto_name}.proto" ABSOLUTE)
get_filename_component(proto_path "${proto}" PATH)

set(proto_py "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}_pb2.py")
set(grpc_py "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}_pb2_grpc.py")

add_custom_command(
	OUTPUT ${proto_py} ${grpc_py}
	COMMAND python3
	ARGS -m grpc_tools.protoc --python_out=${CMAKE_CURRENT_BINARY_DIR}
	--grpc_python_out=${CMAKE_CURRENT_BINARY_DIR} -I ${proto_path} ${proto}
	DEPENDS ${proto})
add_custom_target(grpcTask ALL DEPENDS ${proto_py} ${grpc_py})

install(PROGRAMS ${proto_py} ${grpc_py} DESTINATION opt/fcgi/cgi-bin)

FILE(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach(child ${children})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${child}/cpp/CMakeLists.txt)
        add_subdirectory(${child}/cpp)
    endif()
endforeach()

add_subdirectory(asr/python/ctcdecode-numpy)

macro(install_fcgi_py child)
    set(name "fcgi_${child}")
    set(variadic_args ${ARGN})
    list(LENGTH variadic_args variadic_count)
    if(${variadic_count} GREATER 0)
        list(GET variadic_args 0 name)
    endif()
    message("install_fcgi_py name: ${name}")

    set(fcgi_py ${CMAKE_CURRENT_SOURCE_DIR}/${child}/python/${name}.py)
    string(REPLACE "fcgi_" "" config ${name})
    set(fcgi_py_conf ${CMAKE_CURRENT_SOURCE_DIR}/${child}/python/16-${config}-py.conf)
    if(EXISTS ${fcgi_py})
        install(PROGRAMS ${fcgi_py} DESTINATION opt/fcgi/cgi-bin/)
        install(FILES ${fcgi_py_conf} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/lighttpd/conf-available)
        install_link("../conf-available/16-${config}-py.conf" "${CMAKE_INSTALL_SYSCONFDIR}/lighttpd/conf-enabled/" SYMBOLIC)
    else()
        message(FATAL_ERROR "${fcgi_py} not exist")
    endif()
endmacro()

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/utils/python/fcgi_utils.py DESTINATION opt/fcgi/cgi-bin/)
install_fcgi_py(asr)
install_fcgi_py(classification)
install_fcgi_py(digitalnote)
install_fcgi_py(face-detection fcgi_face_detection)
install_fcgi_py(facial-landmark fcgi_facial_landmark)
install_fcgi_py(formula)
install_fcgi_py(handwritten)
install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/handwritten/python/fcgi_codec.py DESTINATION opt/fcgi/cgi-bin/)
install_fcgi_py(ocr)
install_fcgi_py(policy)
install_fcgi_py(ppocr)
install_fcgi_py(segmentation)
install_fcgi_py(super_resolution)
install_fcgi_py(tts)

install(FILES lighttpd.conf DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/lighttpd)
install(PROGRAMS auth-conf.sh DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/lighttpd)
install(FILES ../secure/http DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/pam.d)
install(PROGRAMS ../runit/lighttpd/run DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/sv/lighttpd)
install_link("../../../sv/lighttpd" "${CMAKE_INSTALL_SYSCONFDIR}/runit/runsvdir/default/" SYMBOLIC)
install_link("/tmp/lighttpd/supervise" "${CMAKE_INSTALL_SYSCONFDIR}/sv/lighttpd/" SYMBOLIC)
if ("$ENV{CONFIG_RESTFUL_SAMPLE}" STREQUAL "y")
    install(DIRECTORY asr/python/asr_utils DESTINATION opt/fcgi/cgi-bin/asr USE_SOURCE_PERMISSIONS)
endif()
