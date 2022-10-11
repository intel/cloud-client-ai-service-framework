# Copyright (C) 2021 Intel Corporation
#

set (OPENVINO_ENTRY_NAME openvinoentry)

set (OPENVINO_ENTRY_HEADERS
     ie_wrapper.hpp
     infer_openvino_entity.hpp
     ../iinfer_entity.hpp
     decrypt.h
    )

set (OPENVINO_ENTRY_SOURCES
     ie_wrapper.cpp
     infer_openvino_entity.cpp
     ../iinfer_entity.cpp
     openvino_entry.cpp
     decrypt.c
    )

find_package(PkgConfig REQUIRED)
pkg_check_modules(Crypto REQUIRED IMPORTED_TARGET libcrypto)

find_package(InferenceEngine REQUIRED)
message("-- Found InferenceEngine ${InferenceEngine_VERSION_MAJOR}.${InferenceEngine_VERSION_MINOR}")
if(${InferenceEngine_VERSION_MAJOR} LESS_EQUAL 2021)
    get_filename_component(IE_DEMO_INCLUDES "${InferenceEngine_INCLUDE_DIRS}/../samples/cpp/common" ABSOLUTE)
    if(${InferenceEngine_VERSION_MAJOR} EQUAL 2021 AND
       ${InferenceEngine_VERSION_MINOR} EQUAL 4)
        string(APPEND IE_DEMO_INCLUDES "/utils/include")
    endif()
else()
    get_filename_component(IE_DEMO_INCLUDES "${InferenceEngine_INCLUDE_DIRS}/../../../samples/cpp/common/utils/include" ABSOLUTE)
endif()

add_library(${OPENVINO_ENTRY_NAME} SHARED ${OPENVINO_ENTRY_SOURCES} ${OPENVINO_ENTRY_HEADERS})
target_link_libraries(${OPENVINO_ENTRY_NAME} PRIVATE
                      ${InferenceEngine_LIBRARIES}
                      ${OpenCV_LIBRARIES}
                      PkgConfig::Crypto)
target_include_directories(${OPENVINO_ENTRY_NAME} PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/..
                           ${IE_DEMO_INCLUDES}
                           ${OpenCV_INCLUDE_DIRS})

install(TARGETS ${OPENVINO_ENTRY_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
