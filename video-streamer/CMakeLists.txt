# Copyright (C) 2020 Intel Corporation
#
cmake_minimum_required(VERSION 3.16)

project(VideoStreamer)

set(CCAI_STREAM_PLUGINDIR "${CMAKE_INSTALL_PREFIX}/usr/lib/ccai_stream/plugins")

if(NOT ENV{DLSTREAMER_DIR} OR NOT ENV{PKG_CONFIG_PATH})
    set(dlst_dir "/opt/intel/dlstreamer")
    if(IS_SYMLINK "${dlst_dir}")
        get_filename_component(dlst_dir "${dlst_dir}" REALPATH)
    endif()
    if(IS_DIRECTORY "${dlst_dir}/lib/pkgconfig")
        set(dirs "${dlst_dir}/lib/pkgconfig")
        if(IS_DIRECTORY "${dlst_dir}/gstreamer/lib/pkgconfig")
            string(JOIN ":" dirs "${dirs}" "${dlst_dir}/gstreamer/lib/pkgconfig")
        endif()
        if(ENV{PKG_CONFIG_PATH})
            string(JOIN ":" dirs "$ENV{PKG_CONFIG_PATH}" "${dirs}")
        endif()
        set(ENV{PKG_CONFIG_PATH} ${dirs})
    endif()
endif()

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GST_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0)
find_package(RuntimeService REQUIRED)
if(NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV REQUIRED core)
endif()

set(CCAI_STREAM ccai_stream)
add_library(${CCAI_STREAM} SHARED ccai_stream.c plugin.c)
target_link_libraries(${CCAI_STREAM} PRIVATE
                      PkgConfig::GSTREAMER
                      Threads::Threads
                      ${CMAKE_DL_LIBS})

target_include_directories(${CCAI_STREAM} PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

install(TARGETS ${CCAI_STREAM}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ccai_stream.h ccai_stream_plugin.h ccai_stream_utils.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


# launcher plugin
pkg_check_modules(JsonC REQUIRED IMPORTED_TARGET json-c)
add_library(general_launcher SHARED general_launcher.c)
set_target_properties(general_launcher PROPERTIES PREFIX "")
target_link_libraries(general_launcher PRIVATE
                      ${CCAI_STREAM}
                      PkgConfig::GSTREAMER
                      PkgConfig::JsonC)
install(TARGETS general_launcher LIBRARY DESTINATION ${CCAI_STREAM_PLUGINDIR})
install(FILES plugins/general_launcher.conf DESTINATION ${CCAI_STREAM_PLUGINDIR})


# background blur plugin
add_library(backgroundblur SHARED backgroundblur.cpp)
set_target_properties(backgroundblur PROPERTIES PREFIX "")
target_link_libraries(backgroundblur PRIVATE ${CCAI_STREAM}
                      CCAI::RuntimeService
                      ${OpenCV_LIBRARIES}
                      PkgConfig::GSTREAMER)
add_dependencies(backgroundblur CCAI::RuntimeService)
install(TARGETS backgroundblur LIBRARY DESTINATION ${CCAI_STREAM_PLUGINDIR})


# emotion plugin
add_library(emotion SHARED emotion.cpp)
set_target_properties(emotion PROPERTIES PREFIX "")
target_include_directories(emotion PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(emotion PRIVATE ${CCAI_STREAM}
                      CCAI::RuntimeService
                      ${OpenCV_LIBRARIES}
                      PkgConfig::GST_VIDEO)
add_dependencies(emotion CCAI::RuntimeService)
install(TARGETS emotion LIBRARY DESTINATION ${CCAI_STREAM_PLUGINDIR})


# pose estimation plugin
pkg_check_modules(DLSTREAMER REQUIRED IMPORTED_TARGET dl-streamer)
add_library(pose SHARED pose.cpp hpe_eHRNet.cpp
            thirdparty/associative_embedding_decoder.cpp
            thirdparty/kuhn_munkres.cpp)
set_target_properties(pose PROPERTIES PREFIX "")
target_include_directories(pose PRIVATE thirdparty)
target_link_libraries(pose PRIVATE ${CCAI_STREAM}
                      CCAI::RuntimeService
                      PkgConfig::DLSTREAMER
                      ${OpenCV_LIBRARIES}
                      PkgConfig::GST_VIDEO)
add_dependencies(pose CCAI::RuntimeService)
install(TARGETS pose LIBRARY DESTINATION ${CCAI_STREAM_PLUGINDIR})
