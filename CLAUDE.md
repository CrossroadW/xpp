# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash

conan install . --output-folder=build --profile:build=conan-release.profile --profile:host=conan-release.profile --remote=conancenter
conan install . --output-folder=build --profile:build=conan-debug.profile --profile:host=conan-debug.profile --remote=conancenter

# Full clean build
rm -rf build && conan install . --output-folder=build --build=missing -s compiler.cppstd=20
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j4

# Incremental build (after code changes)
cmake --build build --config Release -j4

# Run the main server (no external dependencies required)
./build/Release/xpp.exe

# Run simple test (no database required)
./build/Release/test_simple.exe

# Build specific target
cmake --build build --config Release --target xpp

# Run tests
cmake --build build --config Release --target test_logger test_memory_cache test_database_pool test_auth_service
ctest --preset conan-release
```

## Architecture Overview

### Core Design Principles

XPP uses three key architectural patterns:

#### 1. **IoC Container (Inversion of Control)**
- Located in `include/xpp/core/ioc_container.hpp`
- Singleton pattern managing service registration and resolution
- Supports two lifetimes: `Singleton` (one instance) and `Transient` (new instance per resolve)
- **Key usage pattern**:
  ```cpp
  auto& container = xpp::core::IoCContainer::instance();
  container.register_service<UserService>(
      []() { return std::make_shared<UserService>(); },
      xpp::core::IoCContainer::Lifetime::Singleton
  );
  auto service = container.resolve<UserService>();
  ```
- Services registered in `main.cpp::register_modules()` before server startup
- All business logic services should be registered here for loose coupling

#### 2. **Event Bus (Publish-Subscribe)**
- Located in `include/xpp/core/event_bus.hpp`
- Thread-safe event dispatch system for inter-module communication
- Supports synchronous and asynchronous event handling
- **Typical flow**: Module A publishes event → Module B subscribes and handles
- Enables complete module decoupling without direct dependencies

#### 3. **Middleware Chain Pattern**
- HTTP request/response processing pipeline in `include/xpp/network/http_server.hpp`
- Each middleware wraps the next in the chain
- Used for cross-cutting concerns (auth, logging, rate limiting, CORS)
- Middleware stack built in `main.cpp::setup_routes()`

### Layered Architecture

```
┌─────────────────────────────────────┐
│   Modules (Business Logic)          │ include/xpp/modules/
│   ├── user/ (auth, registration)    │ All header-only (.hpp)
│   └── [future modules]              │
├─────────────────────────────────────┤
│   Infrastructure (Data Access)      │ include/xpp/infrastructure/
│   ├── database_pool.hpp             │ SQLite3 connection wrapper
│   └── memory_cache.hpp              │ In-memory session cache
├─────────────────────────────────────┤
│   Network (HTTP/WebSocket)          │ include/xpp/network/
│   └── http_server.hpp               │ Drogon wrapper with middleware
├─────────────────────────────────────┤
│   Core Framework                    │ include/xpp/core/
│   ├── ioc_container.hpp             │ Dependency injection
│   ├── event_bus.hpp                 │ Async event system
│   ├── config_manager.hpp            │ YAML/JSON config
│   └── logger.hpp                    │ spdlog wrapper
└─────────────────────────────────────┘
```

### Header-Only Architecture

**IMPORTANT: All code in `include/` directory MUST be header-only (.hpp files only)**

- All framework components (`xpp/core/`, `xpp/infrastructure/`, `xpp/network/`) are header-only
- **All module implementations (`xpp/modules/`) are now header-only** - moved from `src/modules/`
- Built as CMake `INTERFACE` library in `CMakeLists.txt`
- Enables zero-overhead abstractions, template metaprogramming, and faster incremental builds
- Only `src/main.cpp` and test files should be .cpp files
- **When adding new modules**: Always create them in `include/xpp/modules/<module_name>/` as .hpp files
- **Avoid .cpp files**: Only create .cpp files if absolutely necessary for very large translation units that significantly impact compile times
- Use `#pragma once` for header guards
- Include paths should use `xpp/` prefix (e.g., `#include "xpp/core/logger.hpp"`)

## Database System

### SQLite3 Implementation
- **Current database**: SQLite3 (embedded, no external server required)
- Located in `include/xpp/infrastructure/database_pool.hpp`
- Simple wrapper around SQLite3 C API
- Returns `QueryResult` with rows as `std::vector<std::vector<std::string>>`
- Access columns by index: `result[row_index][column_index]`
- **SQL escaping**: Manual escaping required (see auth_service.hpp for examples)
- **Transactions**: Use `begin_transaction()` API

### Database Usage Patterns

```cpp
auto& db = infrastructure::DatabasePool::instance();

// Simple query
auto result = db.execute_sync("SELECT * FROM users WHERE id = 1");
if (!result.empty()) {
    std::string username = result[0][1];  // Access by column index
}

// With manual SQL escaping (IMPORTANT for security)
auto username_escaped = [&username]() {
    std::string out;
    for (char c : username) {
        if (c == '\'') out += "''"; else out += c;
    }
    return out;
}();
auto result = db.execute_sync(
    fmt::format("SELECT * FROM users WHERE username = '{}'", username_escaped)
);

// Get last insert ID
int64_t user_id = db.last_insert_id();
```

## Logging System

### Logger API
- **DO NOT use LOG_INFO/LOG_WARN/LOG_ERROR macros** - they have been replaced
- **Use inline functions instead**: `xpp::log_info()`, `xpp::log_warn()`, `xpp::log_error()`
- Located in `include/xpp/core/logger.hpp`
- Uses `std::string_view` for format strings (not `fmt::format_string`)

### Correct Usage

```cpp
// ✅ CORRECT
xpp::log_info("User logged in: {}", username);
xpp::log_warn("Invalid request from {}", ip_address);
xpp::log_error("Database error: {}", error_msg);

// ❌ WRONG - Don't use these macros
LOG_INFO("User logged in: {}", username);  // Compilation error
LOG_WARN("Invalid request");               // Compilation error
```

## JSON Handling

### Drogon vs nlohmann::json
- **Drogon uses**: `Json::Value` (jsoncpp library)
- **XPP models use**: `nlohmann::json`
- **Conversion required** in controllers when receiving HTTP requests

### Conversion Pattern

```cpp
// In controller handlers
auto json = req->getJsonObject();  // Returns Json::Value (jsoncpp)

// Convert to nlohmann::json
nlohmann::json nlohmann_json = nlohmann::json::parse(json->toStyledString());
auto request = LoginRequest::from_json(nlohmann_json);

// Response (nlohmann::json → Json::Value handled by Response::json())
callback(Response::success(result->to_json()));
```

### Response Helper
- `network::Response::json()` automatically converts `nlohmann::json` to `Json::Value`
- Located in `include/xpp/network/http_server.hpp`

## Adding New Modules

### Module Structure

**All modules are header-only and live in `include/xpp/modules/<module_name>/`**

```
include/xpp/modules/message/
├── message_model.hpp          # Data structures (requests, responses, entities)
├── message_service.hpp        # Business logic (queries, commands)
├── message_controller.hpp     # HTTP route handlers
└── message_repository.hpp     # (Optional) Database access layer
```

### Step-by-Step Guide

1. **Create module directory**:
   ```bash
   mkdir -p include/xpp/modules/message
   ```

2. **Define models** (message_model.hpp):
   ```cpp
   #pragma once
   #include <nlohmann/json.hpp>

   namespace xpp::modules::message {
   struct Message { int64_t id; std::string content; };
   struct SendMessageRequest {
       int64_t recipient_id;
       std::string text;

       static SendMessageRequest from_json(const nlohmann::json& j) {
           return {
               .recipient_id = j.value("recipient_id", 0LL),
               .text = j.value("text", "")
           };
       }
   };
   }
   ```

3. **Implement service** (message_service.hpp):
   - All implementation inline in the header file
   - Inject dependencies via constructor: `DatabasePool`, `MemoryCache`, `EventBus`
   - Use IoC container to resolve other services
   - Publish domain events via `EventBus::publish()`
   - Use `xpp::log_info()` for logging (not LOG_INFO macro)

4. **Create controller** (message_controller.hpp):
   - All implementation inline in the header file
   - Inject `MessageService` via constructor
   - `register_routes()` method to wire HTTP endpoints
   - Use `network::Response` helper for JSON responses
   - Convert `Json::Value` to `nlohmann::json` when parsing requests

5. **Register in main.cpp**:
   ```cpp
   // Include the module headers
   #include "xpp/modules/message/message_service.hpp"
   #include "xpp/modules/message/message_controller.hpp"

   // In register_modules()
   container.register_service<modules::message::MessageService>(
       []() { return std::make_shared<modules::message::MessageService>(); },
       core::IoCContainer::Lifetime::Singleton
   );

   // In setup_routes()
   auto msg_controller = std::make_shared<modules::message::MessageController>(
       container.resolve<modules::message::MessageService>()
   );
   msg_controller->register_routes(server);
   ```

**Important Notes**:
- All module code must be in header files (.hpp) in `include/xpp/modules/`
- Do NOT create .cpp files in `src/modules/` unless absolutely necessary for compile time optimization
- Use `#pragma once` at the top of every header file
- Include paths must use `xpp/` prefix: `#include "xpp/modules/message/message_model.hpp"`

## Configuration System

- Config file: `config/config.yaml` (YAML format, can also be JSON)
- Access via: `xpp::core::ConfigManager::instance()`
- **Key methods**:
  ```cpp
  auto& config = ConfigManager::instance();
  config.load_yaml("config/config.yaml");
  auto port = config.get<int>("server.port");
  auto host = config.get_or<std::string>("server.host", "0.0.0.0");
  ```
- Nested values use dot notation: `"database.file"`, `"logging.level"`
- All infrastructure services configured from YAML on startup

## Common Compilation Issues

### Issue: "Cannot find header file"
- **Cause**: Include paths using relative paths like `"../../core/logger.hpp"`
- **Fix**: Use absolute includes from include root: `"xpp/core/logger.hpp"`
- All `include/xpp/` headers should be included with `#include "xpp/..."`

### Issue: LOG_INFO/LOG_WARN/LOG_ERROR compilation errors
- **Cause**: Macros have been replaced with inline functions
- **Fix**: Use `xpp::log_info()`, `xpp::log_warn()`, `xpp::log_error()` instead

### Issue: JSON conversion errors (Json::Value vs nlohmann::json)
- **Cause**: Drogon uses jsoncpp, models use nlohmann::json
- **Fix**: Convert using `nlohmann::json::parse(json->toStyledString())`

### Issue: "Drogon not found" or missing dependencies
- **Cause**: Conan dependencies not properly installed
- **Fix**:
  ```bash
  rm -rf build
  conan install . --output-folder=build --build=missing -s compiler.cppstd=20
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
  ```

## Key Files Reference

| File | Purpose |
|------|---------|
| `include/xpp/core/ioc_container.hpp` | Service registration and dependency resolution |
| `include/xpp/core/event_bus.hpp` | Inter-module async communication |
| `include/xpp/core/config_manager.hpp` | Unified config (YAML/JSON) access |
| `include/xpp/core/logger.hpp` | Global logging (use `xpp::log_*()` functions) |
| `include/xpp/network/http_server.hpp` | HTTP routing, middleware, JSON conversion |
| `include/xpp/infrastructure/database_pool.hpp` | SQLite3 connection wrapper |
| `include/xpp/infrastructure/memory_cache.hpp` | In-memory session cache (thread-safe with TTL) |
| `src/main.cpp` | Server initialization, service registration, route setup |
| `include/xpp/modules/user/` | Example module: authentication, JWT, user management (all .hpp) |
| `config/config.yaml` | Server config (host, port, DB file, logging) |

## Testing

- Simple unit test (no database): `./build/Release/test_simple.exe`
- Full test suite: 35 unit tests covering Logger, MemoryCache, DatabasePool, AuthService
- Run specific test: `./build/test_auth_service --gtest_filter="AuthServiceTest.LoginValidCredentials"`
- See `README.md` for complete testing documentation

## Caching System

### Memory Cache (Default)
- Located in `include/xpp/infrastructure/memory_cache.hpp`
- Thread-safe in-process cache with automatic TTL expiration
- Used for storing user session tokens (24-hour TTL)
- Ideal for development, testing, and single-process deployments
- **No external dependencies** - Redis not required

### Cache Usage
- Initialized in `main.cpp::initialize_services()`
- Accessed via: `xpp::infrastructure::MemoryCache::instance()`
- API: `set()`, `get()`, `exists()`, `del()`, `expire()`
- Data is lost on server restart (in-process storage)
