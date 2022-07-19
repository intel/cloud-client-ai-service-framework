# Copyright (C) 2020 Intel Corporation

cmake_minimum_required (VERSION 3.16)
include(FindPkgConfig)
project(gateway)

find_package(OpenCV REQUIRED core imgproc imgcodecs)
if(NOT OpenCV_FOUND)
	message(WARNING "OPENCV is not found")
	return()
endif()

if (USE_LIBPCI)
	pkg_check_modules(LIBPCI libpci)
	if (NOT LIBPCI_FOUND)
		message(WARNING "libpci is not found")
		return()
	endif()
endif()

add_compile_options(-g -O2 -Wall -fPIE -fstack-protector-all -D_FORTIFY_SOURCE=2)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,now -Wl,-z,relro")

set(CCAI_LOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../log)
find_library(CCAI_LOG_LIBRARY
    ccai_log
    ${CCAI_LOG_DIR}
    NO_DEFAULT_PATH)
set(CCAI_LOG_INCLUDE ${CCAI_LOG_DIR})

# add runtime library support
set(LIBRARY_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../../inference-runtime/release/lib)
find_library(RUNTIME_LIBRARY_TARGET_NAME
    inferservice
    ${LIBRARY_FOLDER}
    NO_DEFAULT_PATH)
set (Runtime_LIBRARIES ${RUNTIME_LIBRARY_TARGET_NAME})
set(INFER_SERVICE_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../../inference-runtime/release/include)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

macro(add_fcgi_binary name)
    include_directories(${CMAKE_SOURCE_DIR}/utils/cpp)
    add_executable(${name} ${CMAKE_SOURCE_DIR}/utils/cpp/fcgi_utils.cpp ${name}.cpp)
    target_include_directories(${name} PRIVATE ${CCAI_LOG_INCLUDE})
    target_include_directories(${name} PRIVATE ${INFER_SERVICE_INCLUDE})
    target_link_libraries(${name} PRIVATE ${OpenCV_LIBRARIES})
    target_link_libraries(${name} PRIVATE fcgi)
    target_link_libraries(${name} PRIVATE ${Runtime_LIBRARIES})
    target_link_libraries(${name} PRIVATE ${CCAI_LOG_LIBRARY})
    install(TARGETS ${name} DESTINATION /opt/fcgi/cgi-bin)
    string(REPLACE "fcgi_" "" config ${name})
    install(FILES 16-${config}.conf DESTINATION /etc/lighttpd/conf-available)
endmacro()

macro(add_cap_api_fcgi_binary name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE fcgi)
    install(TARGETS ${name} DESTINATION /opt/fcgi/cgi-bin)
    string(REPLACE "fcgi_" "" config ${name})
    install(FILES 16-${config}.conf DESTINATION /etc/lighttpd/conf-available)
endmacro()

set(proto_name "service_runtime_health_monitor")
get_filename_component(proto "${proto_name}.proto" ABSOLUTE)
get_filename_component(proto_path "${proto}" PATH)

set(proto_py "${CMAKE_SOURCE_DIR}/${proto_name}_pb2.py")
set(grpc_py "${CMAKE_SOURCE_DIR}/${proto_name}_pb2_grpc.py")

add_custom_command(
	OUTPUT ${proto_py} ${grpc_py}
	COMMAND python3
	ARGS -m grpc_tools.protoc --python_out=${CMAKE_SOURCE_DIR}
	--grpc_python_out=${CMAKE_SOURCE_DIR} -I ${proto_path} ${proto}
	DEPENDS ${proto})
add_custom_target(grpcTask ALL DEPENDS ${proto_py} ${grpc_py})

install(PROGRAMS ${proto_py} ${grpc_py} DESTINATION /opt/fcgi/cgi-bin)

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
        install(PROGRAMS ${fcgi_py} DESTINATION /opt/fcgi/cgi-bin/)
        install(FILES ${fcgi_py_conf} DESTINATION /etc/lighttpd/conf-available)
    else()
        message(FATAL_ERROR "${fcgi_py} not exist")
    endif()
endmacro()

install(PROGRAMS utils/python/fcgi_utils.py DESTINATION /opt/fcgi/cgi-bin/)
install_fcgi_py(asr)
install_fcgi_py(classification)
install_fcgi_py(digitalnote)
install_fcgi_py(face-detection fcgi_face_detection)
install_fcgi_py(facial-landmark fcgi_facial_landmark)
install_fcgi_py(formula)
install_fcgi_py(handwritten)
install(PROGRAMS handwritten/python/fcgi_codec.py DESTINATION /opt/fcgi/cgi-bin/)
install_fcgi_py(ocr)
install_fcgi_py(policy)
install_fcgi_py(ppocr)
install_fcgi_py(segmentation)
install_fcgi_py(super_resolution)
install_fcgi_py(tts)
