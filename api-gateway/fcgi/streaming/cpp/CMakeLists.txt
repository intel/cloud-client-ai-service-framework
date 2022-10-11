# Copyright (C) 2020 Intel Corporation
#

if ("$ENV{CONFIG_RESTFUL_SAMPLE}" STREQUAL "y")
	if ("$ENV{CONFIG_OPENVINO}" STREQUAL "y")
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(JsonC REQUIRED IMPORTED_TARGET json-c)
		find_package(VideoStreamer REQUIRED)
		find_package(Threads)

		add_fcgi_binary(fcgi_streaming)
		target_sources(fcgi_streaming PRIVATE limbs_status.cpp)
		target_link_libraries(fcgi_streaming PRIVATE CCAI::VideoStreamer
                                                             PkgConfig::JsonC
                                                             Threads::Threads)
	endif()
endif()
