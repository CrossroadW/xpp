# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Full clean build
rm -rf build && conan install . --output-folder=build --build=missing -s compiler.cppstd=20
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j4

# Incremental build (after code changes)
cmake --build build --config Release -j4

# Run the main server (requires PostgreSQL and Redis running)
./build/Release/xpp.exe

# Run simple test (no database required)
./build/Release/test_simple.exe

# Build specific target
cmake --build build --config Release --target xpp
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
│   Modules (Business Logic)          │ src/modules/
│   ├── user/ (auth, registration)    │
│   └── [future modules]              │
├─────────────────────────────────────┤
│   Infrastructure (Data Access)      │ include/xpp/infrastructure/
│   ├── database_pool.hpp             │ PostgreSQL connection pooling
│   └── redis_client.hpp              │ Redis operations wrapper
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

### Header-Only Core Library

- All `include/xpp/` components are header-only (no .cpp files)
- Built as CMake `INTERFACE` library in `CMakeLists.txt`
- Enables zero-overhead abstractions and template metaprogramming
- Module implementations in `src/modules/` can be .hpp or .cpp files

## Adding New Modules

### Module Structure

Each module lives in `src/modules/<module_name>/` with this pattern:

```
src/modules/message/
├── message_model.hpp          # Data structures (requests, responses, entities)
├── message_service.hpp        # Business logic (queries, commands)
├── message_controller.hpp     # HTTP route handlers
└── message_repository.hpp     # (Optional) Database access layer
```

### Step-by-Step Guide

1. **Create module directory**:
   ```bash
   mkdir -p src/modules/message
   ```

2. **Define models** (message_model.hpp):
   ```cpp
   namespace xpp::modules::message {
   struct Message { int64_t id; std::string content; };
   struct SendMessageRequest { int64_t recipient_id; std::string text; };
   }
   ```

3. **Implement service** (message_service.hpp):
   - Inject dependencies via constructor: `DatabasePool`, `RedisClient`, `EventBus`
   - Use IoC container to resolve other services
   - Publish domain events via `EventBus::publish()`

4. **Create controller** (message_controller.hpp):
   - Inject `MessageService` via constructor
   - `register_routes()` method to wire HTTP endpoints
   - Use `network::Response` helper for JSON responses

5. **Register in main.cpp**:
   ```cpp
   // In register_modules()
   container.register_service<modules::message::MessageService>(
       xpp::core::IoCContainer::Lifetime::Singleton
   );

   // In setup_routes()
   auto msg_controller = std::make_shared<modules::message::MessageController>(
       container.resolve<modules::message::MessageService>()
   );
   msg_controller->register_routes(server);
   ```

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
- Nested values use dot notation: `"database.host"`, `"logging.level"`
- All infrastructure services configured from YAML on startup

## Common Compilation Issues

### Issue: "Cannot find header file"
- **Cause**: Include paths using relative paths like `"../../core/logger.hpp"`
- **Fix**: Use absolute includes from include root: `"xpp/core/logger.hpp"`
- All `include/xpp/` headers should be included with `#include "xpp/..."`

### Issue: "Drogon not found" or missing database drivers
- **Cause**: Conan dependencies not properly installed
- **Fix**:
  ```bash
  rm -rf build
  conan install . --output-folder=build --build=missing -s compiler.cppstd=20
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
  ```

### Issue: Cannot find PostgreSQL or OpenSSL
- **Fix**: These are linked in `CMakeLists.txt` - ensure Conan and CMake configuration match

## Key Files Reference

| File | Purpose |
|------|---------|
| `include/xpp/core/ioc_container.hpp` | Service registration and dependency resolution |
| `include/xpp/core/event_bus.hpp` | Inter-module async communication |
| `include/xpp/core/config_manager.hpp` | Unified config (YAML/JSON) access |
| `include/xpp/core/logger.hpp` | Global logging wrapper around spdlog |
| `include/xpp/network/http_server.hpp` | HTTP routing and middleware chain |
| `include/xpp/infrastructure/database_pool.hpp` | PostgreSQL connection management |
| `include/xpp/infrastructure/redis_client.hpp` | Redis operations wrapper |
| `src/main.cpp` | Server initialization, service registration, route setup |
| `src/modules/user/` | Example module: authentication, JWT, user management |
| `config/config.yaml` | Server config (host, port, DB, Redis, logging) |
| `config/init_db.sql` | Database schema initialization script |

## Testing

- Simple unit test (no database): `./build/Release/test_simple.exe`
- Tests core components: Logger, ConfigManager, IoCContainer, EventBus
- To add more tests: Create test files in `src/` and add `add_executable()` in CMakeLists.txt

## Database Setup

1. Initialize schema:
   ```bash
   psql -U postgres -f config/init_db.sql
   ```

2. Sample user credentials (in initialized DB):
   - Username: `admin`
   - Password: `password123`

3. Default connection: `localhost:5432/xpp_db`

## Performance Considerations

- **Connection pooling**: Database (10 connections by default) and Redis pool configured in `config.yaml`
- **Event system**: Async events run in detached threads; synchronous events block caller
- **Caching**: User sessions cached in Redis for 24 hours after login
- **Middleware**: Each middleware adds overhead; order matters for performance

## Extending Framework (Advanced)

### Adding Custom Middleware

```cpp
server.use([](const auto& req, auto&& callback, auto&& next) {
    // Pre-processing
    LOG_INFO("Request: {}", req->getPath());

    // Pass to next middleware/handler
    next();

    // Post-processing (optional)
});
```

### Subscribing to Domain Events

```cpp
auto& bus = xpp::core::EventBus::instance();
auto sub_id = bus.subscribe<UserRegisteredEvent>([](const UserRegisteredEvent& e) {
    LOG_INFO("New user: {}", e.username);
    // Send welcome email, update analytics, etc.
});
```

### Using Database Transactions

```cpp
auto& db = infrastructure::DatabasePool::instance();
db.transaction([](auto trans) {
    trans->execSqlSync("INSERT INTO users ...");
    trans->execSqlSync("UPDATE statistics ...");
    // Auto-commits if no exceptions; auto-rollbacks on exception
});
```
