# Copyright (C) 2020 Intel Corporation
#

if ("$ENV{CONFIG_RESTFUL_SAMPLE}" STREQUAL "y")
    if ("$ENV{CONFIG_PYTORCH}" STREQUAL "y")
        add_fcgi_binary(fcgi_face_detection_torch)
    endif()
endif()
