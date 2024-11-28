delameta_github_add_package("preprocessor:boostorg/preprocessor#boost-1.86.0")
delameta_github_add_package("googletest:google/googletest#v1.15.2" OPTIONS "INSTALL_GTEST OFF")
delameta_github_add_package("fmt:fmtlib/fmt#11.0.2" OPTIONS "FMT_INSTALL OFF")

file(GLOB_RECURSE TEST_SOURCES *.*)
add_executable(test_all ${TEST_SOURCES})

target_link_libraries(test_all PRIVATE delameta boost_preprocessor fmt gtest gtest_main)
target_compile_options(test_all PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

