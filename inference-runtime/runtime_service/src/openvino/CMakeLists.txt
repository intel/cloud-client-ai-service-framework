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

find_package(InferenceEngine 2.0 REQUIRED)
if (InferenceEngine_FOUND)
    string(REGEX REPLACE "(.+)\\deployment_tools/inference_engine.*" "\\1" IE_PATH ${InferenceEngine_INCLUDE_DIRS})
    message(${IE_PATH})
else()
    message(WARNING "IE not found, " ${IE_SAMPLE_NAME} " skipped")
    return()
endif()

#OPENVINO R3
#set (IE_DEMO_INCLUDES ${IE_PATH}deployment_tools/inference_engine/samples/cpp/common)
#OPENVINO R4
set (IE_DEMO_INCLUDES ${IE_PATH}deployment_tools/inference_engine/samples/cpp/common/utils/include)
set (IE_AUDIO_INCLUDES ${IE_PATH}data_processing/audio/speech_recognition/include)

# add audio library support
message("cmake_current_dir:" ${CMAKE_CURRENT_SOURCE_DIR})
set (LIBRARY_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../../lib)
find_library(SPEECH_LIBRARY_TARGET_NAME
             speech_library
             ${LIBRARY_FOLDER}
             NO_DEFAULT_PATH)
message("ok: " ${SPEECH_LIBRARY_TARGET_NAME})
set (Speech_LIBRARIES ${SPEECH_LIBRARY_TARGET_NAME})

find_library(CRYPTO_LIBRARY_TARGET_NAME
    crypto
    PATHS "/usr/lib/x86_64-linux-gnu/" "/usr/local/lib/"
    NO_DEFAULT_PATH)
message("crypto ok" ${CRYPTO_LIBRARY_TARGET_NAME})
set (Crypto_LIBRARIES ${CRYPTO_LIBRARY_TARGET_NAME})

add_library(${OPENVINO_ENTRY_NAME} SHARED ${OPENVINO_ENTRY_SOURCES} ${OPENVINO_ENTRY_HEADERS})
target_link_libraries(${OPENVINO_ENTRY_NAME} PRIVATE ${Speech_LIBRARIES} ${OpenCV_LIBRARIES}
                                                     ${Curl_LIBRARIES} ${Crypto_LIBRARIES} 
                                                     ${CMAKE_THREAD_LIBS_INIT} ${IMMEDIATE_BINDING_FLAG}
                                                     ${InferenceEngine_LIBRARIES})

target_include_directories(${OPENVINO_ENTRY_NAME} PRIVATE ${IE_DEMO_INCLUDES})
target_include_directories(${OPENVINO_ENTRY_NAME} PRIVATE ${IE_AUDIO_INCLUDES})
target_include_directories(${OPENVINO_ENTRY_NAME} PRIVATE ${Curl_INCLUDES})
target_include_directories(${OPENVINO_ENTRY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

