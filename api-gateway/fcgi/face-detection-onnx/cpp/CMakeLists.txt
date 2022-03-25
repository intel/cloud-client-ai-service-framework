# Copyright (C) 2020 Intel Corporation
#

if ("$ENV{CONFIG_RESTFUL_SAMPLE}" STREQUAL "y")
	if ("$ENV{CONFIG_ONNX}" STREQUAL "y")
		add_fcgi_binary(fcgi_face_detection_onnx)
	endif()
endif()
