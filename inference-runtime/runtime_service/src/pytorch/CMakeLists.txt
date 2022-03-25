# Copyright (C) 2021 Intel Corporation
#

set (TORCH_ENTRY_NAME pytorchentry)

set (TORCH_ENTRY_HEADERS
     torch_wrapper.hpp
     infer_torch_entity.hpp
     ../iinfer_entity.hpp
    )

set (TORCH_ENTRY_SOURCES
     torch_wrapper.cpp
     infer_torch_entity.cpp
     ../iinfer_entity.cpp
     torch_entry.cpp
    )

set (Torch_DIR /opt/libtorch/share/cmake/Torch)
find_package(Torch REQUIRED)

add_library(${TORCH_ENTRY_NAME} SHARED ${TORCH_ENTRY_SOURCES} ${TORCH_ENTRY_HEADERS})
target_link_libraries(${TORCH_ENTRY_NAME} PRIVATE ${TORCH_LIBRARIES} ${OpenCV_LIBRARIES})
target_include_directories(${TORCH_ENTRY_NAME} PRIVATE /opt/libtorch/include)
target_include_directories(${TORCH_ENTRY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

set_property(TARGET ${TORCH_ENTRY_NAME} PROPERTY CXX_STANDARD 14)

