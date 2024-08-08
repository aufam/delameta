include(FetchContent)

# jwt
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        master
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "Disable build example" FORCE)
FetchContent_MakeAvailable(jwt-cpp)

# main app
file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})

target_link_libraries(main PRIVATE ${PROJECT_NAME})
target_link_libraries(main PRIVATE jwt-cpp)
target_compile_options(main PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

