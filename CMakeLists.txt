cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

# project setting
file(READ version.txt DELAMETA_VERSION)
project(delameta VERSION ${DELAMETA_VERSION})

# some options
option(BUILD_APP "Build the main app" ON)

# some messages
message("API version      : ${PROJECT_VERSION}")
message("BUILD_APP       : ${BUILD_APP}")

# build library
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.*)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE
	-Wall
	-Wextra
	$<$<COMPILE_LANGUAGE:CXX>:-Wno-literal-suffix>
)

target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include> 
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
	-DDELAMETA_VERSION="${PROJECT_VERSION}"
	-DDELAMETA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	-DDELAMETA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	-DDELAMETA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# thread dependency
find_package(Threads REQUIRED)
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_OPTIONS "-pthread")
target_link_libraries(${PROJECT_NAME} PUBLIC ${CMAKE_THREAD_LIBS_INIT} stdc++fs)

# etl dependency
find_package(etl CONFIG REQUIRED)

# install target
install(TARGETS ${PROJECT_NAME}
	EXPORT ${PROJECT_NAME}Targets
	LIBRARY DESTINATION lib COMPONENT Runtime
	ARCHIVE DESTINATION lib COMPONENT Development
	RUNTIME DESTINATION bin COMPONENT Runtime
	PUBLIC_HEADER DESTINATION include COMPONENT Development
	BUNDLE DESTINATION bin COMPONENT Runtime
)

# config version
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY ExactVersion
)

# config file
configure_package_config_file(
	"${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
)

# install config
install(EXPORT ${PROJECT_NAME}Targets DESTINATION lib/cmake/${PROJECT_NAME})
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	DESTINATION lib/cmake/${PROJECT_NAME}
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

# main app
if (BUILD_APP)
    add_subdirectory(app)
endif()

