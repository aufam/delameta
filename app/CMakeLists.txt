CPMAddPackage("gh:boostorg/preprocessor#boost-1.86.0")
CPMAddPackage("gh:fmtlib/fmt#11.0.2")
CPMAddPackage("gh:Thalhammer/jwt-cpp@0.7.0")

# main app
file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})

target_compile_features(main PRIVATE cxx_std_23)
target_link_libraries(main PRIVATE delameta boost_preprocessor fmt jwt-cpp)
target_compile_options(main PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

# Define package metadata
set(PACKAGE_NAME "delameta")
set(PACKAGE_VERSION ${DELAMETA_VERSION})
set(PACKAGE_MAINTAINER "Aufa <aufam@github.com>")
set(PACKAGE_DEBIAN_DIR "${CMAKE_BINARY_DIR}/${PACKAGE_NAME}")

# Create debian directory and control files
configure_file(${CMAKE_SOURCE_DIR}/cmake/control.in ${CMAKE_BINARY_DIR}/control @ONLY)

# Custom target to build the Debian package
add_custom_target(build-deb
    # copy the executable
    COMMAND mkdir -p ${PACKAGE_DEBIAN_DIR}/usr/bin
    COMMAND cp ${CMAKE_BINARY_DIR}/app/main -T ${PACKAGE_DEBIAN_DIR}/usr/bin/${PACKAGE_NAME}

    # copy the control script
    COMMAND mkdir -p ${PACKAGE_DEBIAN_DIR}/DEBIAN
    COMMAND cp ${CMAKE_BINARY_DIR}/control ${PACKAGE_DEBIAN_DIR}/DEBIAN

    # copy the assets
    COMMAND mkdir -p ${PACKAGE_DEBIAN_DIR}/usr/share/${PACKAGE_NAME}/assets
    COMMAND cp ${CMAKE_SOURCE_DIR}/app/README.html -T ${PACKAGE_DEBIAN_DIR}/usr/share/${PACKAGE_NAME}/assets/index.html
    COMMAND cp ${CMAKE_SOURCE_DIR}/app/README.md ${PACKAGE_DEBIAN_DIR}/usr/share/${PACKAGE_NAME}/assets

    # build the package
    COMMAND dpkg-deb --build ${PACKAGE_NAME} ${PACKAGE_NAME}-${PACKAGE_VERSION}.deb

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building Debian package"

    DEPENDS delameta main
)

