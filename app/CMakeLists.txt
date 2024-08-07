include(FetchContent)

# jwt
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        master
)

# SQLpp11
FetchContent_Declare(
    sqlpp11
    GIT_REPOSITORY https://github.com/rbock/sqlpp11.git
    GIT_TAG        main 
)

FetchContent_MakeAvailable(jwt-cpp sqlpp11)

# main app
file(GLOB_RECURSE APP_MAIN_SOURCES *.*)
add_executable(main ${APP_MAIN_SOURCES})

target_link_libraries(main PRIVATE ${PROJECT_NAME})
target_link_libraries(main PRIVATE jwt-cpp sqlpp11)
target_compile_options(main PRIVATE -Wall -Wextra -Wno-literal-suffix -Wno-attributes)

