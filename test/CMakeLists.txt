CPMAddPackage("gh:google/googletest@1.15.2")
CPMAddPackage("gh:boostorg/preprocessor#boost-1.86.0")
CPMAddPackage("gh:fmtlib/fmt#11.0.2")

file(GLOB_RECURSE TEST_SOURCES *.*)
add_executable(test_all ${TEST_SOURCES})

target_link_libraries(test_all PRIVATE delameta boost_preprocessor fmt gtest gtest_main)
target_compile_options(test_all PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

