# 项目结构说明

## 目录树

```
xpp/
├── .gitignore                          # Git 忽略配置
├── CMakeLists.txt                      # CMake 构建配置
├── CMakeUserPresets.json               # CMake 预设
├── conanfile.txt                       # Conan 依赖管理 (已移除 redis-plus-plus)
├── conan-debug.profile                 # Conan Debug 配置
├── conan-release.profile               # Conan Release 配置
│
├── README.md                           # 项目概览
├── CLAUDE.md                           # 开发指南 (已更新)
├── BUILD_GUIDE.md                      # ✨ 编译和运行指南 (新)
├── MIGRATION_SUMMARY.md                # ✨ MemoryCache 迁移总结 (新)
├── IMPLEMENTATION_COMPLETE.md          # ✨ 实现完成清单 (新)
├── MEMORY_CACHE_API.md                 # ✨ MemoryCache API 参考 (新)
│
├── include/xpp/                        # 核心库头文件 (header-only)
│   ├── core/
│   │   ├── logger.hpp                  # 日志系统 (spdlog 包装)
│   │   ├── config_manager.hpp          # 配置管理 (YAML/JSON)
│   │   ├── ioc_container.hpp           # IoC 容器 (依赖注入)
│   │   ├── event_bus.hpp               # 事件总线 (发布-订阅)
│   │   └── jwt_service.hpp             # JWT 令牌服务
│   │
│   ├── infrastructure/
│   │   ├── database_pool.hpp           # PostgreSQL 连接池
│   │   └── memory_cache.hpp            # ✨ 内存缓存 (新)
│   │
│   └── network/
│       ├── http_server.hpp             # HTTP 服务器 (Drogon 包装)
│       └── response.hpp                # HTTP 响应助手
│
├── src/                                # 实现代码
│   ├── main.cpp                        # 应用入口 (已更新)
│   ├── test_simple.cpp                 # 单元测试
│   │
│   └── modules/                        # 业务模块
│       └── user/
│           ├── user_model.hpp          # 用户数据模型
│           ├── auth_service.hpp        # 身份验证服务 (已更新)
│           └── auth_controller.hpp     # 身份验证控制器
│
├── config/                             # 配置文件
│   ├── config.yaml                     # 应用配置 (数据库, 日志等)
│   └── init_db.sql                     # 数据库初始化脚本
│
├── docs/                               # 文档
│   └── redis_alternatives.md           # Redis 替代方案指南
│
├── build/                              # 构建输出 (不跟踪)
│   ├── Release/
│   │   ├── xpp.exe                     # 主应用
│   │   ├── test_simple.exe             # 测试应用
│   │   └── ...其他构建文件
│   └── ...CMake/Conan 文件
│
└── logs/                               # 运行时日志 (不跟踪)
    └── xpp.log                         # 应用日志文件
```

---

## 核心组件说明

### 🔧 基础框架 (`include/xpp/core/`)

| 组件 | 文件 | 功能 |
|------|------|------|
| **Logger** | `logger.hpp` | 日志系统，支持控制台和文件输出 |
| **Config** | `config_manager.hpp` | YAML/JSON 配置加载和访问 |
| **IoC** | `ioc_container.hpp` | 依赖注入容器，管理服务生命周期 |
| **EventBus** | `event_bus.hpp` | 发布-订阅事件系统 |
| **JWT** | `jwt_service.hpp` | JWT 令牌生成和验证 |

### 📦 基础设施 (`include/xpp/infrastructure/`)

| 组件 | 文件 | 功能 | 状态 |
|------|------|------|------|
| **DatabasePool** | `database_pool.hpp` | PostgreSQL 连接管理 | 不变 |
| **MemoryCache** | `memory_cache.hpp` | 内存缓存系统 | ✨ 新增 |

**移除**: `redis_client.hpp` (用 MemoryCache 替代)

### 🌐 网络层 (`include/xpp/network/`)

| 组件 | 文件 | 功能 |
|------|------|------|
| **HttpServer** | `http_server.hpp` | HTTP 服务器包装 (Drogon) |
| **Response** | `response.hpp` | HTTP 响应助手 |

### 🎯 业务模块 (`src/modules/`)

#### 用户认证模块 (`src/modules/user/`)

```
user/
├── user_model.hpp            # 数据结构
│   ├── User                  # 用户实体
│   ├── RegisterRequest       # 注册请求
│   ├── LoginRequest          # 登录请求
│   ├── AuthResponse          # 认证响应
│   └── [其他 DTO]
│
├── auth_service.hpp          # 业务逻辑 ✨ 已更新
│   ├── register_user()       # 注册用户
│   ├── login()               # 用户登录
│   ├── logout()              # 用户登出
│   ├── verify_token()        # 验证令牌
│   ├── cache_user_session()  # 缓存会话 (使用 MemoryCache)
│   └── [其他方法]
│
└── auth_controller.hpp       # HTTP 路由
    ├── handle_register()     # POST /api/auth/register
    ├── handle_login()        # POST /api/auth/login
    ├── handle_logout()       # POST /api/auth/logout
    ├── handle_get_current_user() # GET /api/auth/me
    └── [其他端点]
```

---

## 关键改动详解

### 1. MemoryCache 新增

**文件**: `include/xpp/infrastructure/memory_cache.hpp`

- 线程安全的键值存储
- 支持 TTL 自动过期
- 单例模式
- 完全替代 Redis 在项目中的角色

### 2. main.cpp 更新

**变更内容**:
```cpp
// 之前
#include "xpp/infrastructure/redis_client.hpp"
// ...
RedisClient::instance().initialize(redis_config);

// 现在
#include "xpp/infrastructure/memory_cache.hpp"
// ...
MemoryCache::instance().initialize();
```

### 3. auth_service.hpp 更新

**变更位置**:
1. 头文件包含
2. `cache_user_session()` 方法
3. `verify_token()` 方法
4. `logout()` 方法

**都改用 MemoryCache 替代 RedisClient**

### 4. conanfile.txt 更新

**移除依赖**:
```
redis-plus-plus/1.3.12
```

编译不再需要 redis-plus-plus 库。

---

## 模块依赖关系

```
┌─────────────────────────────────────┐
│   main.cpp                          │
│   - 初始化所有服务                  │
│   - 设置路由                        │
└──────────────┬──────────────────────┘
               │
        ┌──────┴──────┬──────────────┐
        ▼             ▼              ▼
   ┌────────┐   ┌─────────┐    ┌─────────┐
   │ Logger │   │  Config │    │   IoC   │
   └────────┘   └─────────┘    └────┬────┘
                                     │
                           ┌─────────┴────────┐
                           ▼                  ▼
                      ┌─────────┐        ┌──────────────┐
                      │EventBus │        │AuthService   │
                      └─────────┘        └──────┬───────┘
                                                │
                        ┌───────────────────────┼───────────────────┐
                        ▼                       ▼                   ▼
                   ┌──────────┐           ┌──────────────┐    ┌────────┐
                   │MemoryCache           │DatabasePool │    │   JWT  │
                   └──────────┘           └──────────────┘    └────────┘
```

---

## 数据流

### 用户登录流程

```
1. 客户端发送登录请求
   POST /api/auth/login
   ▼
2. AuthController.handle_login()
   ▼
3. AuthService.login()
   ├─ DatabasePool: 查询用户
   ├─ 验证密码
   ├─ JwtService: 生成 token
   └─ MemoryCache: 存储会话 (TTL: 24h) ✨ 新
   ▼
4. 返回 token 给客户端
```

### 请求验证流程

```
1. 客户端发送请求 + Authorization 头
   GET /api/auth/me
   Authorization: Bearer <token>
   ▼
2. AuthController.handle_get_current_user()
   ▼
3. AuthService.verify_token()
   ├─ 解析 JWT token
   ├─ MemoryCache: 检查会话 ✨ 新
   ├─ DatabasePool: 获取用户信息
   └─ 返回用户数据
   ▼
4. 返回用户信息给客户端
```

### 用户登出流程

```
1. 客户端发送登出请求
   POST /api/auth/logout
   ▼
2. AuthController.handle_logout()
   ▼
3. AuthService.logout()
   ├─ MemoryCache: 删除会话 ✨ 新
   └─ Token 失效
   ▼
4. 返回成功响应
```

---

## 配置文件结构

### config.yaml

```yaml
server:
  host: "0.0.0.0"              # 监听地址
  port: 8080                   # 监听端口
  threads: 4                   # 工作线程
  enable_cors: true            # CORS 支持

database:
  host: "localhost"
  port: 5432
  database: "xpp_db"
  username: "postgres"
  password: ""
  connection_num: 10

logging:
  level: "info"
  log_dir: "logs"
  max_file_size: 10485760
  max_files: 5

# redis 配置已移除 (使用 MemoryCache)
```

---

## 编译产物

编译后在 `build/Release/` 目录：

```
build/Release/
├── xpp.exe                    # 主应用程序
├── test_simple.exe            # 单元测试
├── xpp.lib                    # 静态库 (如果编译)
└── ...其他库文件
```

---

## 文档映射

| 文档 | 内容 | 读者 |
|------|------|------|
| `README.md` | 项目概览 | 所有人 |
| `CLAUDE.md` | 架构和开发指南 | 开发者 |
| `BUILD_GUIDE.md` | 编译和运行 | 构建工程师 |
| `MIGRATION_SUMMARY.md` | MemoryCache 实现 | 维护者 |
| `MEMORY_CACHE_API.md` | API 参考 | 开发者 |
| `IMPLEMENTATION_COMPLETE.md` | 完成清单 | 项目经理 |

---

## 版本信息

**项目**: XPP WeChat Backend Framework
**版本**: 1.0.0 (MemoryCache Edition)
**最后更新**: 2026-01-09

**关键改动**:
- ✨ 集成 MemoryCache，移除 Redis 依赖
- ✅ 简化开发环境设置
- ✅ 保持完整的认证功能

---

## 常见路径

```cpp
// 获取日志
auto& logger = xpp::core::Logger::instance();

// 获取配置
auto& config = xpp::core::ConfigManager::instance();

// 获取 IoC 容器
auto& container = xpp::core::IoCContainer::instance();

// 获取事件总线
auto& bus = xpp::core::EventBus::instance();

// 获取数据库连接
auto& db = xpp::infrastructure::DatabasePool::instance();

// 获取内存缓存 ✨ 新
auto& cache = xpp::infrastructure::MemoryCache::instance();

// 日志输出（使用新的 function API）
xpp::log_info("Message: {}", value);
xpp::log_error("Error: {}", error);
```

---

祝开发愉快！有问题请参考各个文档文件。
