cmake_minimum_required(VERSION 3.16)
project(SmartPhoto)
include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
find_package(RuntimeService REQUIRED)
if(NOT DEFINED OpenCV_FOUND)
    find_package(OpenCV REQUIRED core imgcodecs)
endif()

add_library(smartphoto SHARED smart_photo.cpp db.cpp)
target_link_libraries(smartphoto PRIVATE
                      ${SQLITE3_LIBRARIES}
                      ${OpenCV_LIBS}
                      CCAI::RuntimeService)
target_include_directories(smartphoto PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
add_dependencies(smartphoto CCAI::RuntimeService)

install(TARGETS smartphoto LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ccai_smart_photo.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


add_executable(test test.cpp)
target_link_libraries(test smartphoto)
