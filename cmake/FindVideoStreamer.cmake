# Copyright (C) 2021 Intel Corporation

add_library(CCAI::VideoStreamer SHARED IMPORTED)

set_target_properties(CCAI::VideoStreamer PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${VideoStreamer_SOURCE_DIR}"
)

# Import target "CCAI::VideoStreamer" for configuration ""
set_property(TARGET CCAI::VideoStreamer APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(CCAI::VideoStreamer PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${VideoStreamer_BINARY_DIR}/libccai_stream.so"
  IMPORTED_SONAME_NOCONFIG "libccai_stream.so"
)
