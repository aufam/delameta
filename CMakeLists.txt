cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)

# project setting
file(READ version.txt DELAMETA_VERSION)
project(delameta VERSION ${DELAMETA_VERSION})

# some messages
message("API version      : ${PROJECT_VERSION}")

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

file(GLOB_RECURSE APP_MAIN_SOURCES app/*.*)
add_executable(main ${APP_MAIN_SOURCES})
target_compile_options(main PRIVATE -Wall -Wextra -Wno-literal-suffix)
target_link_libraries(main PRIVATE ${PROJECT_NAME})