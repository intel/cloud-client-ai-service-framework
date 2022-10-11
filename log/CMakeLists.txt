# Copyright (C) 2021 Intel Corporation

cmake_minimum_required(VERSION 3.16)

project(Log)
include(GNUInstallDirs)

add_library(ccai_log SHARED ccai_log.c log.c ccai_log.h log.h)
target_compile_options(ccai_log PRIVATE "-fvisibility=hidden")
target_include_directories(ccai_log PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

install(TARGETS ccai_log LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ccai_log.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
