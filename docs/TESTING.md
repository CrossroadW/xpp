# Testing Guide for XPP Project

This document describes how to run and write tests for the XPP project.

## Overview

The XPP project uses **Google Test (GTest)** as the testing framework. Tests are organized by module:

- **test_logger.cpp** - Logger functionality tests
- **test_memory_cache.cpp** - In-memory cache tests
- **test_database_pool.cpp** - SQLite database wrapper tests
- **test_auth_service.cpp** - Authentication service tests

## Prerequisites

Before running tests, ensure:

1. **Conan dependencies are installed**:
   ```bash
   conan install . -s build_type=Release --build=missing
   ```

2. **GoogleTest is available** (included in conanfile if needed):
   - Add `gtest/1.14.0` to `conanfile.txt` requires section if not present

## Building Tests

### Using CMake Preset (Recommended)

```bash
# Configure
cmake --preset conan-release

# Build tests
cmake --build --preset conan-release --target test_logger test_memory_cache test_database_pool test_auth_service
```

### Manual CMake Build

```bash
# In the project root
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target test_logger test_memory_cache test_database_pool test_auth_service
```

## Running Tests

### Run All Tests

```bash
# Using CMake
ctest --preset conan-release

# Or directly in build directory
cd build
ctest
```

### Run Specific Test Suite

```bash
# Run only logger tests
./build/test_logger

# Run only memory cache tests
./build/test_memory_cache

# Run only database pool tests
./build/test_database_pool

# Run only auth service tests
./build/test_auth_service
```

### Run Specific Test

```bash
# Using GTest filter
./build/test_logger --gtest_filter="LoggerTest.LogInfoMessage"
```

### Verbose Output

```bash
# Show detailed test output
./build/test_logger --gtest_verbose

# Show only failed tests
./build/test_logger --gtest_filter="*FAIL*"
```

## Test Descriptions

### Logger Tests (test_logger.cpp)

Tests the centralized logging system using spdlog and fmt.

| Test | Purpose |
|------|---------|
| `LoggerInitializes` | Verify logger can be initialized |
| `LogInfoMessage` | Test info-level logging with formatting |
| `LogWarningMessage` | Test warning-level logging |
| `LogErrorMessage` | Test error-level logging |
| `LogDebugMessage` | Test debug-level logging |
| `MultipleLogsWithDifferentTypes` | Test logging various data types |

**Running**: `./build/test_logger`

### Memory Cache Tests (test_memory_cache.cpp)

Tests the in-process cache with TTL support (Redis replacement).

| Test | Purpose |
|------|---------|
| `SetAndGetValue` | Basic set/get operations |
| `GetNonexistentKey` | Verify null handling for missing keys |
| `ExistsKey` | Test key existence checking |
| `DeleteKey` | Test key deletion |
| `ClearAllKeys` | Test clearing entire cache |
| `SizeAfterOperations` | Verify size tracking |
| `OverwriteExistingKey` | Test value updates |
| `Ping` | Test PING command (health check) |
| `TTLExpiration` | Test key expiration after TTL |
| `ThreadSafety` | Test concurrent access from multiple threads |
| `VariousValueTypes` | Test storing different value formats |

**Running**: `./build/test_memory_cache`

**Key Features Tested**:
- Thread-safe operations
- TTL-based expiration
- Concurrent access (10 threads × 100 operations)

### Database Pool Tests (test_database_pool.cpp)

Tests SQLite3 wrapper with connection pooling and transaction support.

| Test | Purpose |
|------|---------|
| `InsertData` | Test INSERT operations |
| `SelectData` | Test SELECT queries |
| `UpdateData` | Test UPDATE operations |
| `DeleteData` | Test DELETE operations |
| `Transaction` | Test transaction commit |
| `GetLastInsertId` | Test retrieving last inserted row ID |
| `EscapedStringInQuery` | Test SQL escaping for special characters |
| `EmptyResult` | Test handling of empty result sets |
| `MultipleRows` | Test retrieving multiple rows |

**Running**: `./build/test_database_pool`

**Key Features Tested**:
- CRUD operations
- Transaction management
- SQL escaping
- Result row access by index

### Auth Service Tests (test_auth_service.cpp)

Tests authentication logic including registration, login, JWT validation, and session caching.

| Test | Purpose |
|------|---------|
| `RegisterNewUser` | Test user registration |
| `RegisterDuplicateUsername` | Verify duplicate username prevention |
| `LoginValidCredentials` | Test login with correct credentials |
| `LoginInvalidPassword` | Verify incorrect password rejection |
| `LoginNonexistentUser` | Verify nonexistent user handling |
| `LogoutUser` | Test session invalidation |
| `JWTTokenValidation` | Test JWT token creation and validation |
| `InvalidToken` | Test invalid token rejection |
| `MultipleUserRegistration` | Test registering multiple users |

**Running**: `./build/test_auth_service`

**Key Features Tested**:
- User registration with password hashing
- Login validation
- JWT token generation and verification
- Session caching with MemoryCache
- Duplicate user prevention

## Test Output Examples

### Successful Run
```
[==========] Running 6 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 6 tests from LoggerTest
[ RUN      ] LoggerTest.LoggerInitializes
[       OK ] LoggerTest.LoggerInitializes (5 ms)
[ RUN      ] LoggerTest.LogInfoMessage
[       OK ] LoggerTest.LogInfoMessage (2 ms)
...
[==========] 6 tests from 1 test suite ran. (18 ms total)
[  PASSED  ] 6 tests.
```

### Failed Test
```
[  FAILED  ] AuthServiceTest.LoginInvalidPassword
d:\workspace\xpp\test\test_auth_service.cpp:95: Failure
Expected: (login_response.has_value()) is false
  Actual: true
```

## Troubleshooting

### GoogleTest Not Found

Add to `conanfile.txt`:
```
gtest/1.14.0
```

Then run:
```bash
conan install . -s build_type=Release --build=missing
```

### Compilation Errors

Ensure all dependencies are built:
```bash
cmake --build --preset conan-release --target all
```

### Database Locked (test_database_pool.cpp)

Tests create and delete temporary databases. If you see "database is locked":

1. Ensure no other process is using the test database
2. Delete `test_database.db` and `auth_test_database.db` manually:
   ```bash
   rm -f test_database.db auth_test_database.db
   ```

### Timeout on ThreadSafety Test

On slower systems, the thread safety test may timeout. Increase timeout:
```bash
ctest --preset conan-release --timeout 60
```

## Adding New Tests

### 1. Create Test File
```cpp
#include <gtest/gtest.h>
#include "xpp/path/to/header.hpp"

class MyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialization
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(MyTest, Description) {
    // Test implementation
    EXPECT_TRUE(condition);
}
```

### 2. Update CMakeLists.txt

Add to `test/CMakeLists.txt`:
```cmake
add_executable(test_mymodule test_mymodule.cpp)
target_link_libraries(test_mymodule GTest::gtest GTest::gtest_main ...)
add_test(NAME MyModuleTests COMMAND test_mymodule)
```

### 3. Run Tests

```bash
cmake --build --preset conan-release
ctest --preset conan-release
```

## Test Coverage

To generate coverage reports (if using GCC/Clang):

```bash
# Build with coverage flags
cmake --preset conan-release -DCMAKE_CXX_FLAGS="--coverage"

# Run tests
ctest --preset conan-release

# Generate report (requires lcov)
lcov --directory build --capture --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

## CI/CD Integration

For GitHub Actions or other CI systems:

```yaml
- name: Configure
  run: cmake --preset conan-release

- name: Build Tests
  run: cmake --build --preset conan-release --target test_logger test_memory_cache test_database_pool test_auth_service

- name: Run Tests
  run: ctest --preset conan-release -V
```

## Best Practices

1. **Use descriptive test names**: `RegisterDuplicateUsername` instead of `Test2`
2. **Test one thing per test**: Keep tests focused and independent
3. **Use SetUp/TearDown**: Always clean up resources
4. **Test error cases**: Don't just test the happy path
5. **Use meaningful assertions**: `EXPECT_EQ(actual, expected)` with clear messages
6. **Keep tests fast**: Avoid long-running operations
7. **Document complex tests**: Add comments for non-obvious logic

## References

- [GoogleTest Documentation](https://google.github.io/googletest/)
- [CMake Testing](https://cmake.org/cmake/help/latest/command/enable_testing.html)
- [GTest Assertions](https://google.github.io/googletest/reference/assertions.html)

