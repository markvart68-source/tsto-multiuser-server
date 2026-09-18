cmake_minimum_required(VERSION 3.20)
project(tsto_multiuser_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

add_executable(tsto-server
    src/main.cpp
    src/config/config.cpp
    src/database/database.cpp
    src/auth/auth_service.cpp
    src/http/api_router.cpp
    src/services/town_service.cpp
)

target_include_directories(tsto-server PRIVATE src)
target_link_libraries(tsto-server PRIVATE SQLite::SQLite3)
target_compile_options(tsto-server PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

configure_file(config/server.example.json config/server.json COPYONLY)
