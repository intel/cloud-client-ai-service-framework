# Copyright (C) 2021 Intel Corporation

add_library(CCAI::RuntimeService SHARED IMPORTED)

set_target_properties(CCAI::RuntimeService PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${RuntimeService_SOURCE_DIR}"
)

# Import target "CCAI::RuntimeService" for configuration ""
set_property(TARGET CCAI::RuntimeService APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(CCAI::RuntimeService PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_NOCONFIG "CCAI::Log"
  IMPORTED_LOCATION_NOCONFIG "${RuntimeService_BINARY_DIR}/libinferservice.so"
  IMPORTED_SONAME_NOCONFIG "libinferservice.so"
)
