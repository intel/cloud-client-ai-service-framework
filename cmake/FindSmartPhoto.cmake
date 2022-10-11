# Copyright (C) 2021 Intel Corporation

add_library(CCAI::SmartPhoto SHARED IMPORTED)

set_target_properties(CCAI::SmartPhoto PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${SmartPhoto_SOURCE_DIR}"
)

# Import target "CCAI::SmartPhoto" for configuration ""
set_property(TARGET CCAI::SmartPhoto APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(CCAI::SmartPhoto PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NOCONFIG "CCAI::RuntimeService;opencv_core;opencv_imgcodecs;sqlite3"
  IMPORTED_LOCATION_NOCONFIG "${SmartPhoto_BINARY_DIR}/libsmartphoto.so"
  IMPORTED_SONAME_NOCONFIG "libsmartphoto.so"
)
