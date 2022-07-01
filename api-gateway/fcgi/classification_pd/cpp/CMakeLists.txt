# Copyright (C) 2020 Intel Corporation
#

if ("$ENV{CONFIG_RESTFUL_SAMPLE}" STREQUAL "y")
	if ("$ENV{CONFIG_PADDLE}" STREQUAL "y")
		add_fcgi_binary(fcgi_classification_pd)
	endif()
endif()
