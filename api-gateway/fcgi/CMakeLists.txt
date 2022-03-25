# Copyright (C) 2020 Intel Corporation
#

cmake_minimum_required (VERSION 2.8.12)
include(FindPkgConfig)
project(gateway)

find_package(OpenCV REQUIRED)
if(NOT OpenCV_FOUND)
	message(WARNING "OPENCV is not found")
	return()
endif()
find_package(InferenceEngine REQUIRED)
if (NOT InferenceEngine_FOUND)
	message(WARNING "InferenceEngine is not found")
	return()
endif()
find_package(CURL REQUIRED)
if (NOT CURL_FOUND)
	message(WARNING "CURL is not found")
	return()
endif()

if (USE_LIBPCI)
	pkg_check_modules(LIBPCI libpci)
	if (NOT LIBPCI_FOUND)
		message(WARNING "libpci is not found")
		return()
	endif()
endif()

find_package(PythonInterp 3.6 REQUIRED)
find_package(PythonLibs REQUIRED)
if (PythonLibs_FOUND)
    message("ok: " ${PYTHON_LIBRARY}) #Python library and python include are specific name
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
    target_include_directories(${name} PRIVATE ${InferenceEngine_INCLUDE_DIRS}/../samples/cpp/common/format_reader)
    target_include_directories(${name} PRIVATE ${InferenceEngine_INCLUDE_DIRS}/../samples/cpp/common/utils/include)
    target_link_libraries(${name} PRIVATE ${OpenCV_LIBRARIES} ${InferenceEngine_LIBRARIES})
    target_link_libraries(${name} PRIVATE fcgi)
    target_link_libraries(${name} PRIVATE ${CURL_LIBRARY})
    target_link_libraries(${name} PRIVATE ${Runtime_LIBRARIES})
    target_link_libraries(${name} PRIVATE ${PYTHON_LIBRARY})
    target_link_libraries(${name} PRIVATE ${CCAI_LOG_LIBRARY})
    target_link_libraries(${name} PRIVATE -L${CMAKE_SOURCE_DIR}/../library)
    install(TARGETS ${name} DESTINATION /opt/fcgi/cgi-bin)
endmacro()

macro(add_cap_api_fcgi_binary name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE fcgi)
    install(TARGETS ${name} DESTINATION /opt/fcgi/cgi-bin)
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

FILE(GLOB children relative ${my_current_dir} ${my_current_dir} / *)
foreach(child ${children})
    if (EXISTS ${my_current_dir}/${child}/cpp/CMakeLists.txt)
        add_subdirectory(${child}/cpp)
    endif()
endforeach()
