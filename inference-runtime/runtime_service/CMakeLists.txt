# Copyright (C) 2020 Intel Corporation
#
cmake_minimum_required(VERSION 3.12)
project(RuntimeService)

if (CMAKE_BUILD_TYPE STREQUAL "")
    message(STATUS "CMAKE_BUILD_TYPE not defined, 'Release' will be used")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if (NOT(BIN_FOLDER))
    string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} ARCH)
    if(ARCH STREQUAL "x86_64" OR ARCH STREQUAL "amd64")
        set(ARCH intel64)
        set(LIBRARY_FOLDER lib/x64)
    elseif(ARCH STREQUAL "i386")
        set(ARCH ia32)
        set(LIBRARY_FOLDER lib/x86)
    endif()

    set (BIN_FOLDER ${ARCH})
endif()

set (COMMON_API_MAIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${COMMON_API_MAIN_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${COMMON_API_MAIN_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib)
set (CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${COMMON_API_MAIN_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE})
set (CMAKE_PDB_OUTPUT_DIRECTORY ${COMMON_API_MAIN_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${COMMON_API_MAIN_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE})
set (LIBRARY_OUTPUT_DIRECTORY ${COMMON_API_MAIN_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib)
set (LIBRARY_OUTPUT_PATH ${LIBRARY_OUTPUT_DIRECTORY}/lib)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror") #treating warnings as errors
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized -Winit-self -std=c++11 -fPIC")
if(NOT ${CMAKE_CXX_COMPILER_ID} STREQUAL Clang)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmaybe-uninitialized")
endif()

add_compile_options(-fstack-protector-all -D_FORTIFY_SOURCE=2 -O2)
set(IMMEDIATE_BINDING_FLAG "-Wl,-z,now -Wl,-z,relro")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${IMMEDIATE_BINDING_FLAG}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${IMMEDIATE_BINDING_FLAG}")


add_subdirectory(src)
