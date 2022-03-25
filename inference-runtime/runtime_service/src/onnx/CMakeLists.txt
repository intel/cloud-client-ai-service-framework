# Copyright (C) 2021 Intel Corporation
#

set (ONNX_ENTRY_NAME onnxentry)

set (ONNX_ENTRY_HEADERS
    onnx_wrapper.hpp
    infer_onnx_entity.hpp
    ../iinfer_entity.hpp
    )

set (ONNX_ENTRY_SOURCES
    onnx_wrapper.cpp
    infer_onnx_entity.cpp
    ../iinfer_entity.cpp
    onnx_entry.cpp
    )

set(onnxruntime_INSTALL_PREFIX /opt/onnxruntime)
set(onnxruntime_INCLUDE_DIRS
    ${onnxruntime_INSTALL_PREFIX}/include
    ${onnxruntime_INSTALL_PREFIX}/include/onnxruntime
    ${onnxruntime_INSTALL_PREFIX}/include/onnxruntime/core/session
)
set(ONNX_LIBRARY_FOLDER ${onnxruntime_INSTALL_PREFIX}/lib)
find_library(onnxruntime_LIBS
             onnxruntime
             ${ONNX_LIBRARY_FOLDER}
             NO_DEFAULT_PATH)

add_library(${ONNX_ENTRY_NAME} SHARED ${ONNX_ENTRY_SOURCES} ${ONNX_ENTRY_HEADERS})
target_link_libraries(${ONNX_ENTRY_NAME} PRIVATE ${onnxruntime_LIBS} ${OpenCV_LIBRARIES})
target_include_directories(${ONNX_ENTRY_NAME} PRIVATE ${onnxruntime_INCLUDE_DIRS})
target_include_directories(${ONNX_ENTRY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
