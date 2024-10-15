option(DELAMETA_APP_STATIC_BUILD "Enable static build" OFF)
message(STATUS "DELAMETA_APP_STATIC_BUILD : ${DELAMETA_APP_STATIC_BUILD}")

# dependencies
CPMAddPackage("gh:boostorg/preprocessor#boost-1.86.0")
CPMAddPackage("gh:fmtlib/fmt#11.0.2")
CPMAddPackage("gh:Thalhammer/jwt-cpp@0.6.0")

# main app
file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})
set_target_properties(main PROPERTIES OUTPUT_NAME delameta)

target_link_libraries(main PRIVATE delameta boost_preprocessor fmt jwt-cpp)
target_compile_options(main PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

# static build
if (DELAMETA_APP_STATIC_BUILD)
    target_link_options(main PRIVATE -static)
endif ()

install(
    TARGETS main
    DESTINATION ${CMAKE_INSTALL_BINDIR}
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

