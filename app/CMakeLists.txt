delameta_find_or_fetch_library(
    boost_preprocessor
    https://github.com/boostorg/preprocessor.git
    master
)

set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "Disable build example" FORCE)
delameta_find_or_fetch_library(
    jwt-cpp
    https://github.com/Thalhammer/jwt-cpp.git
    master
)

delameta_find_or_fetch_library(
    fmt
    https://github.com/fmtlib/fmt.git
    master
)

# main app
file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})

target_compile_features(main PRIVATE cxx_std_23)
target_link_libraries(main PRIVATE delameta jwt-cpp boost_preprocessor fmt)
target_compile_options(main PRIVATE -Wall -Wextra -pedantic -Wno-literal-suffix -Wno-attributes)

