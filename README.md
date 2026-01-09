# XPP - 高性能微信后台系统

基于 Modern C++ (C++20) 的极简高性能微信后台系统框架

## 技术栈

- **语言**: C++20
- **构建系统**: CMake + Conan
- **HTTP框架**: Drogon
- **数据库**: PostgreSQL (libpq)
- **缓存**: Redis (redis-plus-plus)
- **序列化**: Protobuf, nlohmann/json
- **配置**: YAML-cpp
- **日志**: spdlog

## 核心特性

### 1. 核心框架
- ✅ **IoC容器**: 轻量级依赖注入，支持单例和瞬态生命周期
- ✅ **事件总线**: 线程安全的发布-订阅模式，支持异步事件
- ✅ **配置系统**: 统一配置管理，支持YAML和JSON
- ✅ **日志系统**: 基于spdlog的高性能日志，支持控制台和文件输出

### 2. 基础服务
- ✅ **网络层**: 基于Drogon的HTTP/WebSocket服务器封装
- ✅ **数据库连接池**: PostgreSQL连接池，支持事务
- ✅ **Redis客户端**: Redis操作封装，支持常用数据结构

### 3. 业务模块
- ✅ **用户认证**: JWT认证、注册、登录、会话管理

## 项目结构

```
xpp/
├── include/xpp/              # 公共头文件
│   ├── core/                 # 核心组件
│   │   ├── ioc_container.hpp
│   │   ├── event_bus.hpp
│   │   ├── config_manager.hpp
│   │   └── logger.hpp
│   ├── network/              # 网络层
│   │   └── http_server.hpp
│   ├── infrastructure/       # 基础设施
│   │   ├── database_pool.hpp
│   │   └── redis_client.hpp
│   └── middleware/           # 中间件
├── src/                      # 源代码
│   ├── core/
│   ├── network/
│   ├── infrastructure/
│   ├── modules/              # 业务模块
│   │   └── user/            # 用户模块
│   └── main.cpp
├── config/                   # 配置文件
│   ├── config.yaml
│   └── init_db.sql
├── CMakeLists.txt
└── conanfile.txt
```

## 快速开始

### 环境要求

- C++20 编译器 (GCC 10+, Clang 11+, MSVC 19.29+)
- CMake 3.15+
- Conan 2.0+
- PostgreSQL 12+
- Redis 6+

### 安装依赖

```bash
# 安装 Conan 依赖
conan install . --output-folder=build --build=missing

# 配置 CMake
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . --config Release
```

### 初始化数据库

```bash
# 创建数据库并初始化表结构
psql -U postgres -f config/init_db.sql
```

### 配置

编辑 `config/config.yaml`:

```yaml
server:
  host: "0.0.0.0"
  port: 8080
  threads: 4

database:
  host: "localhost"
  port: 5432
  database: "xpp_db"
  username: "postgres"
  password: "your_password"

redis:
  host: "localhost"
  port: 6379
```

### 运行

```bash
./build/xpp
```

服务器将在 `http://localhost:8080` 启动

## API 文档

### 认证接口

#### 注册用户
```http
POST /api/auth/register
Content-Type: application/json

{
  "username": "testuser",
  "password": "password123",
  "email": "test@example.com"
}
```

#### 登录
```http
POST /api/auth/login
Content-Type: application/json

{
  "username": "testuser",
  "password": "password123"
}

Response:
{
  "success": true,
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
    "user": {
      "id": 1,
      "username": "testuser",
      "email": "test@example.com"
    }
  }
}
```

#### 获取当前用户信息
```http
GET /api/auth/me
Authorization: Bearer <token>
```

#### 登出
```http
POST /api/auth/logout
Authorization: Bearer <token>
```

### 健康检查
```http
GET /health

Response:
{
  "success": true,
  "data": {
    "status": "ok",
    "timestamp": 1234567890
  }
}
```

## 架构设计

### IoC容器使用示例

```cpp
auto& container = xpp::core::IoCContainer::instance();

// 注册服务
container.register_service<IUserService, UserService>(
    xpp::core::IoCContainer::Lifetime::Singleton
);

// 解析服务
auto service = container.resolve<IUserService>();
```

### 事件总线使用示例

```cpp
auto& bus = xpp::core::EventBus::instance();

// 订阅事件
auto sub_id = bus.subscribe<UserLoginEvent>([](const UserLoginEvent& event) {
    LOG_INFO("User logged in: {}", event.username);
});

// 发布事件
bus.publish(UserLoginEvent{user_id, username});

// 取消订阅
bus.unsubscribe(sub_id);
```

### 配置系统使用示例

```cpp
auto& config = xpp::core::ConfigManager::instance();

// 加载配置
config.load_yaml("config/config.yaml");

// 读取配置
auto port = config.get<int>("server.port");
auto host = config.get_or<std::string>("server.host", "0.0.0.0");
```

## 扩展开发

### 添加新模块

1. 在 `src/modules/` 下创建模块目录
2. 实现业务逻辑和控制器
3. 在 `main.cpp` 中注册模块和路由

示例：

```cpp
// src/modules/message/message_service.hpp
class MessageService {
public:
    void send_message(int64_t sender_id, int64_t receiver_id, const std::string& content);
};

// src/modules/message/message_controller.hpp
class MessageController {
public:
    void register_routes(HttpServer& server) {
        server.post("/api/messages/send", [this](auto req, auto callback) {
            // 处理消息发送
        });
    }
};

// 在 main.cpp 中注册
container.register_service<MessageService>();
auto msg_controller = std::make_shared<MessageController>(
    container.resolve<MessageService>()
);
msg_controller->register_routes(server);
```

## 性能优化

### 已实现的优化

- **连接池**: 数据库和Redis连接复用
- **异步IO**: 基于Drogon的异步请求处理
- **事件驱动**: 解耦模块间通信
- **缓存**: Redis缓存用户会话

### 待优化项

- [ ] 对象池管理
- [ ] 协程支持 (C++20 coroutines)
- [ ] 消息队列集成 (Kafka/RabbitMQ)
- [ ] 分布式追踪
- [ ] 性能监控和指标收集

## 下一步开发计划

### 即将实现的功能

1. **消息模块**
   - 点对点消息
   - 群组消息
   - 消息持久化

2. **群组管理**
   - 创建/解散群组
   - 成员管理
   - 权限控制

3. **实时通信**
   - WebSocket 支持
   - 在线状态管理
   - 消息推送

4. **文件服务**
   - 文件上传/下载
   - 图片/视频处理
   - CDN 集成

5. **RPC 支务**
   - gRPC/brpc 集成
   - 服务间通信

## 测试

```bash
# 编译测试
cmake -DBUILD_TESTS=ON ..
make
ctest
```

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request！

## 联系方式

- 项目地址: [GitHub](https://github.com/yourusername/xpp)
