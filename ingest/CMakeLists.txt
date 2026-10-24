cmake_minimum_required(VERSION 3.20)
project(echovault_ingest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(mongocxx REQUIRED CONFIG)
find_package(CURL REQUIRED)
find_package(Catch2 3 REQUIRED CONFIG)

add_executable(echovault src/main.cpp)
target_link_libraries(echovault PRIVATE mongo::mongocxx_shared CURL::libcurl)
target_compile_options(echovault PRIVATE -Wall -Wextra)

enable_testing()
add_executable(echovault_tests tests/smoke_test.cpp)
target_link_libraries(echovault_tests PRIVATE mongo::bsoncxx_shared Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(echovault_tests)
