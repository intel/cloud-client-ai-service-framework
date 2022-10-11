# Copyright (C) 2021 Intel Corporation
#

cmake_minimum_required(VERSION 3.16)
project(RuntimeService)
include(GNUInstallDirs)

add_library(inferservice SHARED
            connect_server.cpp
            cpu_state.cpp cpu_state.hpp
            iinfer_entity.hpp iinfer_entity.cpp infer_entity.hpp
            irt_inference_engine.cpp irt_inference_engine.hpp
            policy.cpp policy.hpp
            shared_lib_load.cpp shared_lib_load.hpp shared_lib_pointer.hpp
            vino_ie_pipe.cpp vino_ie_pipe.hpp)

find_package(CURL REQUIRED)
find_package(Log REQUIRED)
if (NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV REQUIRED core imgcodecs)
endif()

target_link_libraries(inferservice PRIVATE
                      CCAI::Log
                      CURL::libcurl
                      ${CMAKE_DL_LIBS})
target_include_directories(inferservice PRIVATE ${OpenCV_INCLUDE_DIRS})

target_include_directories(inferservice PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

install(TARGETS inferservice LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES inference_engine_library.txt DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
install(FILES inference_engine_library.txt DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES vino_ie_pipe.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Backend Plugins
add_subdirectory(openvino)
add_subdirectory(onnx)
add_subdirectory(tensorflow)
add_subdirectory(pytorch)
add_subdirectory(paddle)


# Policy Daemon
find_package(Threads REQUIRED)
add_executable(policy_daemon
               daemon_policy.cpp
               cpu_state.cpp cpu_state.hpp
               policy.cpp policy.hpp)
target_link_libraries(policy_daemon PRIVATE Threads::Threads)

install(TARGETS policy_daemon RUNTIME DESTINATION "${CMAKE_INSTALL_SBINDIR}")
install(FILES policy_setting.cfg DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
install(FILES "${CMAKE_SOURCE_DIR}/service_runtime_binary/lfs/rootfs/etc/sudoers.d/99-www-data"
        DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/sudoers.d")
install(PROGRAMS "../../runit/policy_daemon/run"
        DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/sv/policy_daemon")
install_link("../../../sv/policy_daemon" "${CMAKE_INSTALL_SYSCONFDIR}/runit/runsvdir/default/" SYMBOLIC)
install_link("/tmp/policy_daemon/supervise" "${CMAKE_INSTALL_SYSCONFDIR}/sv/policy_daemon/" SYMBOLIC)


# Encryption Tool
add_executable(encrypt encrypt.c)
target_link_libraries(encrypt crypto)
install(TARGETS encrypt RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")


# Python Binding
find_package(pybind11 REQUIRED)
find_package(Python3 COMPONENTS Development REQUIRED)
pybind11_add_module(inferservice_python vino_ie_pipe_py.cpp vino_ie_pipe.hpp)
target_link_libraries(inferservice_python PRIVATE
                      inferservice
                      ${OpenCV_LIBRARIES})
target_include_directories(inferservice_python PRIVATE
                           ${OpenCV_INCLUDE_DIRS}
                           ${Python3_INCLUDE_DIRS})
install(TARGETS inferservice_python LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
