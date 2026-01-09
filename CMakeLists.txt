cmake_minimum_required(VERSION 3.15)
project(xpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(spdlog REQUIRED)
find_package(Drogon REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)
find_package(redis++ REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(Protobuf REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Header-only core library (INTERFACE)
add_library(xpp_core INTERFACE)
target_include_directories(xpp_core INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(xpp_core INTERFACE
    spdlog::spdlog
    Drogon::Drogon
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
    redis++::redis++_static
    PostgreSQL::PostgreSQL
    protobuf::libprotobuf
)
target_compile_features(xpp_core INTERFACE cxx_std_20)

# Main executable
add_executable(xpp src/main.cpp)
target_link_libraries(xpp PRIVATE xpp_core)

# Link OpenSSL for JWT (required by auth_service)
find_package(OpenSSL REQUIRED)
target_link_libraries(xpp PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Simple test executable (no database/redis required)
add_executable(test_simple src/test_simple.cpp)
target_link_libraries(test_simple PRIVATE xpp_core)

# Tests (optional)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
