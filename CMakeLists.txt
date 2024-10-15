cmake_minimum_required(VERSION 3.14)

# project setting
file(READ version.txt DELAMETA_VERSION)
project(delameta
	VERSION ${DELAMETA_VERSION}
	DESCRIPTION "C++ framework for socket and serial programming"
)

if (DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD LESS 17)
	message(FATAL_ERROR "Required minimum standard is C++17")
endif()

# some options
option(DELAMETA_BUILD_APP 	 "Build the main app"  OFF)
option(DELAMETA_BUILD_TEST 	 "Build the test"	   OFF)
option(DELAMETA_TARGET_LINUX "Build for Linux"	   ON)
option(DELAMETA_TARGET_STM32 "Build for STM32"	   OFF)

# some messages
message(STATUS "DELAMETA_VERSION      : ${PROJECT_VERSION}")
message(STATUS "DELAMETA_BUILD_APP    : ${DELAMETA_BUILD_APP}")
message(STATUS "DELAMETA_BUILD_TEST   : ${DELAMETA_BUILD_TEST}")
message(STATUS "DELAMETA_TARGET_LINUX : ${DELAMETA_TARGET_LINUX}")
message(STATUS "DELAMETA_TARGET_STM32 : ${DELAMETA_TARGET_STM32}")

# ensure that only one target is selected
if (DELAMETA_TARGET_LINUX AND DELAMETA_TARGET_STM32)
    message(FATAL_ERROR "Please choose either DELAMETA_TARGET_LINUX or DELAMETA_TARGET_STM32, not both.")
elseif (NOT DELAMETA_TARGET_LINUX AND NOT DELAMETA_TARGET_STM32)
    message(FATAL_ERROR "Please select a target: DELAMETA_TARGET_LINUX or DELAMETA_TARGET_STM32.")
endif()

# cpm cmake
include(cmake/CPM.cmake)

# build library
if (DELAMETA_TARGET_LINUX)
	file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.* core/linux/*.*)
	add_library(delameta ${SOURCES})
	
	target_compile_options(delameta PRIVATE
		-Wall
		-Wextra
		-pedantic
		-Wconversion
		-Wsign-conversion
		-Wnull-dereference
		$<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>
		$<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
		$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
	)
	target_include_directories(delameta PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include> 
	)
	target_compile_definitions(delameta PUBLIC
		-DDELAMETA_VERSION="${DELAMETA_VERSION}"
		-DDELAMETA_HOME_DIRECTORY="${CMAKE_SOURCE_DIR}"
	)

	# thread dependency
	find_package(Threads REQUIRED)
	set_target_properties(delameta PROPERTIES COMPILE_OPTIONS "-pthread")
	target_link_libraries(delameta PUBLIC ${CMAKE_THREAD_LIBS_INIT} stdc++fs)

	# openssl dependency
	set(OPENSSL_USE_STATIC_LIBS TRUE)
	find_package(OpenSSL REQUIRED)
	target_link_libraries(delameta PUBLIC OpenSSL::SSL OpenSSL::Crypto)

	# etl dependency
	if (NOT TARGET etl)
		CPMAddPackage("gh:aufam/etl#main")
	endif ()
	target_link_libraries(delameta PUBLIC etl)
endif (DELAMETA_TARGET_LINUX)

if (DELAMETA_TARGET_STM32)
	file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.* core/stm32_hal/*.*)
	add_library(delameta ${SOURCES})

	target_compile_options(delameta PRIVATE
		-Wall
		-Wextra
		-pedantic
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
		-DDELAMETA_VERSION="${DELAMETA_VERSION}"
		-DDELAMETA_HOME_DIRECTORY="${CMAKE_SOURCE_DIR}"
	)

	# etl dependency
	if (NOT TARGET etl)
		CPMAddPackage("gh:aufam/etl#FreeRTOS")
	endif ()
	target_link_libraries(delameta PUBLIC etl)
endif (DELAMETA_TARGET_STM32)

# main app
if (DELAMETA_TARGET_LINUX AND DELAMETA_BUILD_APP)
	add_subdirectory(app)
endif()

# test
if (DELAMETA_TARGET_LINUX AND DELAMETA_BUILD_TEST)
	add_subdirectory(test)
	enable_testing()
	add_test(NAME test_all COMMAND test_all)
endif()

# installation config
install(TARGETS delameta
	EXPORT delametaTargets
	LIBRARY DESTINATION lib COMPONENT Runtime
	ARCHIVE DESTINATION lib COMPONENT Development
	RUNTIME DESTINATION bin COMPONENT Runtime
	PUBLIC_HEADER DESTINATION include COMPONENT Development
	BUNDLE DESTINATION bin COMPONENT Runtime
)

install(DIRECTORY include/ DESTINATION include)
install(EXPORT delametaTargets DESTINATION lib/cmake/delameta)

# package config and versioning
include(CMakePackageConfigHelpers)
configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/delametaConfig.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/delametaConfig.cmake"
	INSTALL_DESTINATION lib/cmake/delameta
)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/delametaConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/delametaConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/delametaConfigVersion.cmake"
	DESTINATION lib/cmake/delameta
)

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
