delameta_find_or_fetch_library(
    boost_preprocessor
    https://github.com/boostorg/preprocessor.git
    master
)

delameta_find_or_fetch_library(
    fmt
    https://github.com/fmtlib/fmt.git
    master
)

delameta_find_or_fetch_library(
    GTest
    https://github.com/google/googletest.git
    main
)

file(GLOB_RECURSE TEST_SOURCES *.*)
add_executable(test_all ${TEST_SOURCES})

target_compile_features(test_all PRIVATE cxx_std_23)
target_link_libraries(test_all PRIVATE delameta boost_preprocessor fmt gtest gtest_main)
target_compile_options(test_all PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

