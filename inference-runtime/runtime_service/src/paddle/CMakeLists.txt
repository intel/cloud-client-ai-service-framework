# Copyright (C) 2021 Intel Corporation

set (PADDLE_ENTRY_NAME paddleentry)

set (PADDLE_ENTRY_HEADERS
     paddle_wrapper.hpp
     infer_paddle_entity.hpp
     ../iinfer_entity.hpp
    )

set (PADDLE_ENTRY_SOURCES
     paddle_wrapper.cpp
     infer_paddle_entity.cpp
     ../iinfer_entity.cpp
     paddle_entry.cpp
    )

add_library(${PADDLE_ENTRY_NAME} SHARED ${PADDLE_ENTRY_SOURCES} ${PADDLE_ENTRY_HEADERS})
target_link_libraries(${PADDLE_ENTRY_NAME} PRIVATE ${PADDLE_LIB}/paddle/lib/libpaddle_inference.so ${OpenCV_LIBRARIES})
target_include_directories(${PADDLE_ENTRY_NAME} PRIVATE ${PADDLE_LIB}/paddle/include)
target_include_directories(${PADDLE_ENTRY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

install(TARGETS ${PADDLE_ENTRY_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
