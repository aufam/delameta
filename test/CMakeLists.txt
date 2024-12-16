delameta_github_package("googletest:google/googletest#v1.15.2" OPTIONS "INSTALL_GTEST OFF")

file(GLOB_RECURSE TEST_SOURCES *.*)
add_executable(test_all ${TEST_SOURCES})

target_include_directories(test_all PRIVATE
    "${preprocessor_SOURCE_DIR}/include"
)

target_link_libraries(test_all PRIVATE
    delameta
    fmt-header-only
    gtest
    gtest_main
)

target_compile_options(test_all PRIVATE
    -Wall
    -Wextra
    -Wno-literal-suffix
    -Wno-attributes
    -fmacro-prefix-map=${CMAKE_HOME_DIRECTORY}/=
)

