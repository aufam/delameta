file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})
set_target_properties(main PROPERTIES OUTPUT_NAME delameta)

target_include_directories(main PRIVATE
    "${preprocessor_SOURCE_DIR}/include"
)

target_link_libraries(main PRIVATE
    delameta
    fmt-header-only
)

target_compile_options(main PRIVATE
    -Wall
    -Wextra
    -Wno-literal-suffix
    -Wno-attributes
    -fmacro-prefix-map=${CMAKE_HOME_DIRECTORY}/=
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

