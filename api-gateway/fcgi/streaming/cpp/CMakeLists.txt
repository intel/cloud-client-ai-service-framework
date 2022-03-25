# Copyright (C) 2020 Intel Corporation
#

if ("$ENV{CONFIG_RESTFUL_SAMPLE}" STREQUAL "y")
	if ("$ENV{CONFIG_OPENVINO}" STREQUAL "y")
		find_library(ccai_stream_LIBRARY NAMES libccai_stream.so)

		if (NOT ccai_stream_LIBRARY)
			message("ccai_stream is not found: "${CMAKE_FIND_ROOT_PATH})
			message(FATAL_ERROR "ccai_stream is not found")
			return()
		endif()

		pkg_check_modules(JSON_C json-c)
		if (NOT JSON_C_FOUND)
			message(FATAL_ERROR "json-c is not found")
			return()
		endif()
		find_path(ccai_stream_INCLULDE NAMES ccai_stream.h)
		if (NOT ccai_stream_INCLULDE)
			message(FATAL_ERROR "ccai_stream.h is not found")
			return()
		endif()

		find_package(Threads)

		add_fcgi_binary(fcgi_streaming)
		target_sources(fcgi_streaming PRIVATE limbs_status.cpp)
		target_include_directories(fcgi_streaming PRIVATE ${ccai_stream_INCLULDE})
		target_link_libraries(fcgi_streaming PRIVATE ${ccai_stream_LIBRARY})
		target_link_libraries(fcgi_streaming PRIVATE ${JSON_C_LIBRARIES})
		target_link_libraries(fcgi_streaming PRIVATE ${CMAKE_THREAD_LIBS_INIT})
	endif()
endif()
