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
target_link_options(main PRIVATE -static-libstdc++)

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

# cpack
set(CPACK_PACKAGE_NAME delameta)
set(CPACK_PACKAGE_VERSION ${delameta_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${delameta_DESCRIPTION})

set(CPACK_PACKAGE_VENDOR "Delameta Bilano")
set(CPACK_PACKAGE_CONTACT "aufam@github.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Aufa")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)
