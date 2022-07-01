# Copyright (C) 2020 - 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

find_package (Python3 REQUIRED COMPONENTS Interpreter Development NumPy)

set(target_name ctcdecode_numpy_impl)

set(COMPILER_IS_GCC_LIKE TRUE)
add_compile_options(-Wall -fPIC)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

source_group("include" FILES ${HEADERS})
source_group("src" FILES ${SOURCES})

add_library(${target_name} MODULE ${HEADERS} ${SOURCES})

target_include_directories(${target_name} PRIVATE
    ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS} third_party/ThreadPool)
target_link_libraries(${target_name} ${Python3_LIBRARIES})

set_target_properties(${target_name} PROPERTIES
    PREFIX ""
    OUTPUT_NAME _impl
)

# SWIG-generated code causes some warnings; disable them.
if(COMPILER_IS_GCC_LIKE)
    target_compile_options(${target_name} PRIVATE -Wno-narrowing)
elseif(MSVC)
    target_compile_options(${target_name} PRIVATE
        /wd4244  # Disable conversion warning =
        /wd4838  # Disable conversion warning in aggregate or list initialization
    )
endif()

install(TARGETS ${target_name} DESTINATION /opt/fcgi/cgi-bin/asr/ctcdecode_numpy)

set(init_py  "${CMAKE_CURRENT_SOURCE_DIR}/ctcdecode_numpy/__init__.py")
set(imp_py   "${CMAKE_CURRENT_SOURCE_DIR}/ctcdecode_numpy/impl.py")
install(PROGRAMS ${init_py} ${imp_py} DESTINATION /opt/fcgi/cgi-bin/asr/ctcdecode_numpy)
