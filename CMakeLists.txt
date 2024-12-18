cmake_minimum_required(VERSION 3.14)

# project setting
project(delameta
	VERSION		0.2.0
	DESCRIPTION "C++ framework for socket and serial programming"
)

if (DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 17)
	message(FATAL_ERROR "Required minimum standard is C++17")
endif()

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message(FATAL_ERROR "${CMAKE_CXX_COMPILER_ID} is not supported")
endif()

# some options
option(DELAMETA_BUILD_APP 	    "Build the main app"		   OFF)
option(DELAMETA_BUILD_TEST 	    "Build the test"			   OFF)
option(DELAMETA_INSTALL         "Generate install target"	   ON)
option(DELAMETA_TARGET_STM32    "Build for STM32"			   OFF)
option(DELAMETA_DISABLE_OPENSSL "Disable openssl"			   OFF)

# some messages
message(STATUS "DELAMETA_VERSION         : ${delameta_VERSION}")
message(STATUS "DELAMETA_BUILD_APP       : ${DELAMETA_BUILD_APP}")
message(STATUS "DELAMETA_BUILD_TEST      : ${DELAMETA_BUILD_TEST}")
message(STATUS "DELAMETA_INSTALL         : ${DELAMETA_INSTALL}")
message(STATUS "DELAMETA_TARGET_STM32    : ${DELAMETA_TARGET_STM32}")
message(STATUS "DELAMETA_DISABLE_OPENSSL : ${DELAMETA_DISABLE_OPENSSL}")

# private dependencies
include(cmake/delameta.cmake)
delameta_github_package("fmt:fmtlib/fmt#11.0.2" OPTIONS "FMT_INSTALL OFF")
delameta_github_package("preprocessor:boostorg/preprocessor#boost-1.86.0" DOWNLOAD_ONLY YES)

# build library
if (NOT DELAMETA_TARGET_STM32)
	file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.* core/linux/*.*)
	add_library(delameta ${SOURCES})

	if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
		target_link_libraries(delameta PUBLIC
			ws2_32
		)
	endif()

	target_compile_options(delameta PRIVATE
		-Wall
		-Wextra
		-pedantic
		-Wnull-dereference
		-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-unknown-pragmas>
		$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
		$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
	)

	target_include_directories(delameta PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include> 
	)

	target_compile_definitions(delameta PUBLIC
		-DDELAMETA_VERSION="${delameta_VERSION}"
		-DDELAMETA_HOME_DIRECTORY="${CMAKE_SOURCE_DIR}"
	)

	# thread dependency
	find_package(Threads REQUIRED)
	target_link_libraries(delameta PUBLIC Threads::Threads)

	# check if stdc++fs is needed (for GCC < 9)
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
		target_link_libraries(delameta PUBLIC stdc++fs)
	endif()

	# openssl dependency
	find_package(OpenSSL)
	if (OpenSSL_FOUND AND NOT DELAMETA_DISABLE_OPENSSL)
		target_link_libraries(delameta PRIVATE
			OpenSSL::SSL
		)
	else ()
		if (NOT DELAMETA_DISABLE_OPENSSL)
			message(WARNING "OpenSSL is not found. Disabling OpenSSL...")
		endif()
		target_compile_definitions(delameta PRIVATE
			-DDELAMETA_DISABLE_OPENSSL=1
		)
	endif()

	# etl dependency
	if (NOT TARGET etl)
		delameta_github_package("etl:aufam/etl#main" OPTIONS "ETL_INSTALL ${DELAMETA_INSTALL}")
	endif ()
	target_link_libraries(delameta PUBLIC etl)

	# fmt dependency
	target_link_libraries(delameta PRIVATE fmt-header-only)

	# preprocessor
	target_include_directories(delameta PRIVATE "${preprocessor_SOURCE_DIR}/include")

else()
	file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.* core/stm32_hal/*.*)
	add_library(delameta ${SOURCES})

	target_compile_options(delameta PRIVATE
		-Wall
		-Wextra
		-pedantic
		-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=
		$<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter> # for ethernet
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>
		$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
		$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
	)

	target_include_directories(delameta PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	)

	target_include_directories(delameta PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/core/stm32_hal/Ethernet
	)

	target_compile_definitions(delameta PUBLIC
		-DDELAMETA_VERSION="${delameta_VERSION}"
		-DDELAMETA_HOME_DIRECTORY="${CMAKE_SOURCE_DIR}"
	)

	# etl dependency
	if (NOT TARGET etl)
		delameta_github_package("etl:aufam/etl#FreeRTOS")
	endif ()
	target_link_libraries(delameta PUBLIC etl)

	# fmt dependency
	target_link_libraries(delameta PRIVATE fmt-header-only)

	# preprocessor
	target_include_directories(delameta PRIVATE "${preprocessor_SOURCE_DIR}/include")
endif()

# install
if (NOT DELAMETA_TARGET_STM32 AND DELAMETA_INSTALL)
	# config
	include(CMakePackageConfigHelpers)
	configure_package_config_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/delametaConfig.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/delametaConfig.cmake"
		INSTALL_DESTINATION lib/cmake/delameta
	)

	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/delametaConfig.cmake"
		DESTINATION lib/cmake/delameta
	)

	# include dir
	install(
		DIRECTORY include/
		DESTINATION include
	)

	# targets
	install(
		TARGETS delameta
				fmt-header-only
		EXPORT delametaTargets
		LIBRARY DESTINATION lib COMPONENT Runtime
		ARCHIVE DESTINATION lib COMPONENT Development
		RUNTIME DESTINATION bin COMPONENT Runtime
		PUBLIC_HEADER DESTINATION include COMPONENT Development
		BUNDLE DESTINATION bin COMPONENT Runtime
	)

	install(
		EXPORT delametaTargets
		DESTINATION lib/cmake/delameta
	)

	# versioning
	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/delametaConfigVersion.cmake"
		VERSION ${PROJECT_VERSION}
		COMPATIBILITY AnyNewerVersion
	)

	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/delametaConfigVersion.cmake"
		DESTINATION lib/cmake/delameta
	)
endif()

# uninstall
if(NOT TARGET uninstall)
	configure_file(
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in"
		"${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
		IMMEDIATE @ONLY
	)

	add_custom_target(uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake
	)
endif()

# main app
if (NOT DELAMETA_TARGET_STM32 AND DELAMETA_BUILD_APP)
	add_subdirectory(app)
endif()

# test
if (NOT DELAMETA_TARGET_STM32 AND DELAMETA_BUILD_TEST)
	add_subdirectory(test)
	enable_testing()
	add_test(NAME test_all COMMAND test_all)
endif()

# cpack
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(CPACK_PACKAGE_NAME delameta)
	set(CPACK_PACKAGE_VERSION ${delameta_VERSION})
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${delameta_DESCRIPTION})

	set(CPACK_PACKAGE_VENDOR "Delameta Bilano")
	set(CPACK_PACKAGE_CONTACT "aufam@github.com")
	set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Aufa")

	set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
	include(CPack)
endif()
