cmake_minimum_required(VERSION 3.15)
project(xpp_tests CXX)

set(CMAKE_CXX_STANDARD 20)

# Include the parent project's include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Find required packages
find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(drogon REQUIRED)

# Logger tests
add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger
    GTest::gtest
    GTest::gtest_main
    xpp_core
)

# Memory cache tests
add_executable(test_memory_cache test_memory_cache.cpp)
target_link_libraries(test_memory_cache
    GTest::gtest
    GTest::gtest_main
    xpp_core
)

# Database pool tests
add_executable(test_database_pool test_database_pool.cpp)
target_link_libraries(test_database_pool
    GTest::gtest
    GTest::gtest_main
    xpp_core
    SQLite::SQLite3
    fmt::fmt
)

# Auth service tests
add_executable(test_auth_service test_auth_service.cpp)
target_link_libraries(test_auth_service
    GTest::gtest
    GTest::gtest_main
    xpp_core
    SQLite::SQLite3
    fmt::fmt
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    OpenSSL::SSL
    Drogon::Drogon
)

# API integration tests
add_executable(api_tests api_tests.cpp)
target_link_libraries(api_tests
    GTest::gtest
    xpp_core
    SQLite::SQLite3
    fmt::fmt
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    OpenSSL::SSL
    Drogon::Drogon
)

# Enable testing
enable_testing()

# Add tests
add_test(NAME LoggerTests COMMAND test_logger)
add_test(NAME MemoryCacheTests COMMAND test_memory_cache)
add_test(NAME DatabasePoolTests COMMAND test_database_pool)
add_test(NAME AuthServiceTests COMMAND test_auth_service)
add_test(NAME ApiTests COMMAND api_tests)


# copy config dir to build directory
add_custom_command(TARGET api_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different 
    ${CMAKE_SOURCE_DIR}/config $<TARGET_FILE_DIR:api_tests>/config
)