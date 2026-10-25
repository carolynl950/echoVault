cmake_minimum_required(VERSION 3.20)
project(echovault_ingest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(mongocxx REQUIRED CONFIG)
find_package(CURL REQUIRED)
# Homebrew's OpenSSL is keg-only; point CMake at it if no location was given.
if(APPLE AND NOT OPENSSL_ROOT_DIR AND EXISTS /opt/homebrew/opt/openssl@4)
  set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl@4)
endif()
find_package(OpenSSL REQUIRED)
find_package(Catch2 3 REQUIRED CONFIG)

# Logic that doesn't touch the database, shared by the CLI and the tests.
add_library(echovault_core src/splitter.cpp src/hash.cpp src/diff.cpp src/vault.cpp src/notion_blocks.cpp)
target_include_directories(echovault_core PUBLIC src)
target_link_libraries(echovault_core PUBLIC OpenSSL::Crypto mongo::bsoncxx_shared)
target_compile_options(echovault_core PRIVATE -Wall -Wextra)

add_executable(echovault src/main.cpp src/store.cpp src/notion.cpp)
target_link_libraries(echovault PRIVATE echovault_core mongo::mongocxx_shared CURL::libcurl)
target_compile_options(echovault PRIVATE -Wall -Wextra)

enable_testing()
add_executable(echovault_tests tests/smoke_test.cpp tests/splitter_test.cpp tests/sync_test.cpp tests/notion_test.cpp)
target_link_libraries(echovault_tests PRIVATE echovault_core mongo::bsoncxx_shared Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(echovault_tests)
