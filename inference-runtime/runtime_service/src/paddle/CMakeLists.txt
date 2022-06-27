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

set (PADDLE_LIB /opt/paddle_inference)
# set (OPENBLAS_DIR /opt/paddle_inference/third_party/install/openblas/lib/cmake/openblas/)
# if(WITH_MKL)
#   set(FLAG_OPENMP "-fopenmp")
# endif()
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${FLAG_OPENMP}")

# Add openblas, if needed 
set(PADDLE_LIB_THIRD_PARTY_PATH "${PADDLE_LIB}/third_party/install/")
# set(OPENBLAS_LIB_PATH "${PADDLE_LIB_THIRD_PARTY_PATH}openblas")
# include_directories("${OPENBLAS_LIB_PATH}/include/openblas")
#set(MATH_LIB ${OPENBLAS_LIB_PATH}/lib/libopenblas${CMAKE_STATIC_LIBRARY_SUFFIX})

find_package(OpenCV QUIET)
# if(WITH_STATIC_LIB)
#   set(DEPS ${PADDLE_LIB}/paddle/lib/libpaddle_inference${CMAKE_STATIC_LIBRARY_SUFFIX})
# else()
#   if(WIN32)
#     set(DEPS ${PADDLE_LIB}/paddle/lib/paddle_inference${CMAKE_STATIC_LIBRARY_SUFFIX})
#   else()
#     set(DEPS ${PADDLE_LIB}/paddle/lib/libpaddle_inference${CMAKE_SHARED_LIBRARY_SUFFIX})
#   endif()
# endif()

set(EXTERNAL_LIB "-lrt -ldl -lpthread")
set(DEPS ${DEPS}
    # ${MATH_LIB} 
    ${MKLDNN_LIB} 
    #${OPENBLAS_LIB_PATH}
    ${EXTERNAL_LIB})

# set include
include_directories("${PADDLE_LIB}/")

link_directories("${PADDLE_LIB}/paddle/lib")



add_library(${PADDLE_ENTRY_NAME} SHARED ${PADDLE_ENTRY_SOURCES} ${PADDLE_ENTRY_HEADERS})
target_link_libraries(${PADDLE_ENTRY_NAME} PRIVATE ${DEPS} ${PADDLE_LIB}/paddle/lib/libpaddle_inference.so ${OpenCV_LIBRARIES})
target_include_directories(${PADDLE_ENTRY_NAME} PRIVATE ${PADDLE_LIB}/paddle/include)
target_include_directories(${PADDLE_ENTRY_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)

# set_property(TARGET ${PADDLE_ENTRY_NAME} PROPERTY CXX_STANDARD 14)

