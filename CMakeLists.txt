cmake_minimum_required(VERSION 3.14)

# project setting
file(READ version.txt DELAMETA_VERSION)
project(delameta VERSION ${DELAMETA_VERSION})

# some options
option(DELAMETA_BUILD_APP 	 "Build the main app" ON)
option(DELAMETA_BUILD_TEST 	 "Build the test" ON)
option(DELAMETA_TARGET_LINUX "Build for Linux" ON)
option(DELAMETA_TARGET_STM32 "Build for STM32" OFF)

# Ensure that only one target is selected
if (DELAMETA_TARGET_LINUX AND DELAMETA_TARGET_STM32)
    message(FATAL_ERROR "Please choose either DELAMETA_TARGET_LINUX or DELAMETA_TARGET_STM32, not both.")
elseif (NOT DELAMETA_TARGET_LINUX AND NOT DELAMETA_TARGET_STM32)
    message(FATAL_ERROR "Please select a target: DELAMETA_TARGET_LINUX or DELAMETA_TARGET_STM32.")
endif()

# some messages
message("Delameta version      : ${PROJECT_VERSION}")
message("DELAMETA_BUILD_APP    : ${DELAMETA_BUILD_APP}")
message("DELAMETA_TARGET_LINUX : ${DELAMETA_TARGET_LINUX}")
message("DELAMETA_TARGET_STM32 : ${DELAMETA_TARGET_STM32}")

# find or fetch
include(FetchContent)
function(delameta_find_or_fetch_library LIB_NAME GIT_REPOSITORY GIT_TAG)
    # Check if the library is provided by the parent project
    if(NOT TARGET ${LIB_NAME})
        # Try to find the library installed system-wide
        find_package(${LIB_NAME} QUIET)

        # If not found, fetch it using FetchContent
        if(NOT ${LIB_NAME}_FOUND)
            message(STATUS "Fetching ${LIB_NAME} from ${GIT_REPOSITORY}...")
            FetchContent_Declare(
                ${LIB_NAME}
                GIT_REPOSITORY ${GIT_REPOSITORY}
                GIT_TAG ${GIT_TAG}
            )
            FetchContent_MakeAvailable(${LIB_NAME})
        else()
            message(STATUS "${LIB_NAME} ${${LIB_NAME}_VERSION} found system-wide.")
        endif()
    else()
        message(STATUS "${LIB_NAME} ${${LIB_NAME}_VERSION} found in the parent project.")
    endif()
endfunction()

if (DELAMETA_TARGET_LINUX)
	# build library
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

	# etl dependency
	delameta_find_or_fetch_library(etl https://github.com/aufam/etl.git main)
	target_link_libraries(delameta PUBLIC etl)

	# install target
	install(TARGETS delameta
		EXPORT delametaTargets
		LIBRARY DESTINATION lib COMPONENT Runtime
		ARCHIVE DESTINATION lib COMPONENT Development
		RUNTIME DESTINATION bin COMPONENT Runtime
		PUBLIC_HEADER DESTINATION include COMPONENT Development
		BUNDLE DESTINATION bin COMPONENT Runtime
	)

	# config version
	include(CMakePackageConfigHelpers)
	write_basic_package_version_file(
		"${PROJECT_BINARY_DIR}/delametaConfigVersion.cmake"
		VERSION ${PROJECT_VERSION}
		COMPATIBILITY ExactVersion
	)

	# config file
	configure_package_config_file(
		"${PROJECT_SOURCE_DIR}/cmake/delametaConfig.cmake.in"
		"${PROJECT_BINARY_DIR}/delametaConfig.cmake"
		INSTALL_DESTINATION lib/cmake/delameta
	)

	# install config
	install(EXPORT delametaTargets DESTINATION lib/cmake/delameta)
	install(FILES "${PROJECT_BINARY_DIR}/delametaConfigVersion.cmake"
		"${PROJECT_BINARY_DIR}/delametaConfig.cmake"
		DESTINATION lib/cmake/delameta
	)
	install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)

	# uninstall all
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

endif (DELAMETA_TARGET_LINUX)

if (DELAMETA_TARGET_STM32)
	# build library
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
	delameta_find_or_fetch_library(etl https://github.com/aufam/etl.git FreeRTOS)
	target_link_libraries(delameta PUBLIC etl)
endif (DELAMETA_TARGET_STM32)

# main app
if (DELAMETA_TARGET_LINUX AND DELAMETA_BUILD_APP)
	add_subdirectory(app)
endif()

# main app
if (DELAMETA_TARGET_LINUX AND DELAMETA_BUILD_TEST)
	add_subdirectory(test)
	enable_testing()
	add_test(NAME test_all COMMAND test_all)
endif()