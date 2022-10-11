# Copyright (C) 2020 Intel Corporation
#

if (USE_LIBPCI)
    include(FindPkgConfig)
    pkg_check_modules(LIBPCI REQUIRED IMPORTED_TARGET libpci)
endif()

if ("$ENV{CONFIG_RESTFUL_CAPABILITY}" STREQUAL "y")
	add_cap_api_fcgi_binary(fcgi_apiinfo)
	add_cap_api_fcgi_binary(fcgi_cpuinfo)
	add_cap_api_fcgi_binary(fcgi_meminfo)
	add_cap_api_fcgi_binary(fcgi_gpuinfo)
	if (USE_LIBPCI)
		target_compile_definitions(fcgi_gpuinfo PRIVATE USE_LIBPCI=1)
		target_link_libraries(fcgi_gpuinfo PRIVATE PkgConfig::LIBPCI)
	endif()
endif()
