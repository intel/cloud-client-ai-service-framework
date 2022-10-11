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

find_package(Torch REQUIRED)

add_library(${TORCH_ENTRY_NAME} SHARED ${TORCH_ENTRY_SOURCES} ${TORCH_ENTRY_HEADERS})
target_link_libraries(${TORCH_ENTRY_NAME} PRIVATE ${TORCH_LIBRARIES} ${OpenCV_LIBRARIES})
target_include_directories(${TORCH_ENTRY_NAME} PRIVATE ${TORCH_INCLUDE_DIRS})
target_include_directories(${TORCH_ENTRY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_compile_options(${TORCH_ENTRY_NAME} PRIVATE ${TORCH_CXX_FLAGS})

install(TARGETS ${TORCH_ENTRY_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
