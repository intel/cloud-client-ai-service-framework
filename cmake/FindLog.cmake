# Copyright (C) 2021 Intel Corporation

add_library(CCAI::Log SHARED IMPORTED)

set_target_properties(CCAI::Log PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${Log_SOURCE_DIR}"
)

# Import target "CCAI::Log" for configuration ""
set_property(TARGET CCAI::Log APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(CCAI::Log PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${Log_BINARY_DIR}/libccai_log.so"
  IMPORTED_SONAME_NOCONFIG "libccai_log.so"
)
