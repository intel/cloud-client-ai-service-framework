# Copyright (C) 2021 Intel Corporation
#

set (TF_ENTRY_NAME tensorflowentry)

set (TF_ENTRY_HEADERS
     tf_wrapper.hpp
     infer_tf_entity.hpp
     ../iinfer_entity.hpp
    )

set (TF_ENTRY_SOURCES
     tf_wrapper.cpp
     infer_tf_entity.cpp
     ../iinfer_entity.cpp
     tf_entry.cpp
    )

set(tensorflow_INSTALL_PREFIX /opt/tensorflow)
set(tensorflow_INCLUDE_DIRS ${tensorflow_INSTALL_PREFIX}/include)
set(tensorflow_LIBRARY_FOLDER ${tensorflow_INSTALL_PREFIX}/lib)
find_library(tensorflow_LIBS
             tensorflow
             ${tensorflow_LIBRARY_FOLDER}
             NO_DEFAULT_PATH)

add_library(${TF_ENTRY_NAME} SHARED ${TF_ENTRY_SOURCES} ${TF_ENTRY_HEADERS})
target_link_libraries(${TF_ENTRY_NAME} PRIVATE ${tensorflow_LIBS} ${OpenCV_LIBRARIES})
target_include_directories(${TF_ENTRY_NAME} PRIVATE ${tensorflow_INCLUDE_DIRS})
target_include_directories(${TF_ENTRY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

