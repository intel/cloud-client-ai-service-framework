cmake_minimum_required (VERSION 3.12)
project (smartphoto)

find_package (PkgConfig REQUIRED)
pkg_check_modules (SQLITE3 REQUIRED sqlite3)
find_package (OpenCV REQUIRED core imgproc imgcodecs)
set (INFER_SERVICE_LIB ${CMAKE_SOURCE_DIR}/../inference-runtime/release/lib)
set (INFER_SERVICE_INCLUDE ${CMAKE_SOURCE_DIR}/../inference-runtime/release/include)
find_library (RUNTIME_LIBRARY_TARGET_NAME
    inferservice
    ${INFER_SERVICE_LIB}
    NO_DEFAULT_PATH)
set (Runtime_LIBRARIES ${RUNTIME_LIBRARY_TARGET_NAME})

message ("ok: " ${SQLITE3_LIBRARIES})
message ("ok: " ${OpenCV_LIBS})

set (CMAKE_CXX_STANDARD 17)
add_compile_options (-Wall -Werror -g -O2 -Wno-unused-function
	-I ${INFER_SERVICE_INCLUDE})
add_library (smartphoto SHARED smart_photo.cpp db.cpp)
target_link_libraries (smartphoto ${SQLITE3_LIBRARIES}
	${OpenCV_LIBS}
	${Runtime_LIBRARIES}
	)
set_target_properties (smartphoto PROPERTIES PUBLIC_HEADER ccai_smart_photo.h)
include(GNUInstallDirs)
message ("install lib dir:" ${CMAKE_INSTALL_LIBDIR})
install (TARGETS smartphoto PUBLIC_HEADER)

add_executable (test test.cpp)
target_link_libraries (test smartphoto)
