option(DELAMETA_APP_STATIC_BUILD "Enable static build" OFF)
message(STATUS "DELAMETA_APP_STATIC_BUILD : ${DELAMETA_APP_STATIC_BUILD}")

# dependencies
delameta_github_add_package("preprocessor:boostorg/preprocessor#boost-1.86.0")
delameta_github_add_package("fmt:fmtlib/fmt#11.0.2" OPTIONS "FMT_INSTALL OFF")

# main app
file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})
set_target_properties(main PROPERTIES OUTPUT_NAME delameta)

target_link_libraries(main PRIVATE
    delameta
    boost_preprocessor
    fmt
)

target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -Wno-literal-suffix
    -Wno-attributes
)

install(
    TARGETS main
    DESTINATION bin
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.html
    DESTINATION /usr/share/delameta/assets
    RENAME index.html
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    DESTINATION /usr/share/delameta/assets
)

