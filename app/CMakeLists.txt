include(FetchContent)

# boost/preprocessor
FetchContent_Declare(
    boost_preprocessor
    GIT_REPOSITORY https://github.com/boostorg/preprocessor.git
    GIT_TAG        master
)

# jwt
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        master
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "Disable build example" FORCE)
FetchContent_MakeAvailable(jwt-cpp boost_preprocessor)

# main app
file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})

target_link_libraries(main PRIVATE ${PROJECT_NAME})
target_link_libraries(main PRIVATE jwt-cpp boost_preprocessor)
target_compile_options(main PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

