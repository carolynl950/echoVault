cmake_minimum_required(VERSION 3.20)
project(echovault_ingest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(mongocxx REQUIRED CONFIG)
find_package(CURL REQUIRED)
find_package(Catch2 3 REQUIRED CONFIG)

# Pure logic (no I/O), shared by the CLI and the tests.
add_library(echovault_core src/splitter.cpp)
target_include_directories(echovault_core PUBLIC src)
target_compile_options(echovault_core PRIVATE -Wall -Wextra)

add_executable(echovault src/main.cpp)
target_link_libraries(echovault PRIVATE echovault_core mongo::mongocxx_shared CURL::libcurl)
target_compile_options(echovault PRIVATE -Wall -Wextra)

enable_testing()
add_executable(echovault_tests tests/smoke_test.cpp tests/splitter_test.cpp)
target_link_libraries(echovault_tests PRIVATE echovault_core mongo::bsoncxx_shared Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(echovault_tests)
