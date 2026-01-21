# MemoryCache API 参考

## 概述

`MemoryCache` 是一个线程安全的内存缓存实现，提供键值存储和自动 TTL 过期功能。

**位置**: `include/xpp/infrastructure/memory_cache.hpp`

**命名空间**: `xpp::infrastructure`

**使用模式**: Singleton

---

## 初始化

```cpp
// 获取单例实例
auto& cache = xpp::infrastructure::MemoryCache::instance();

// 初始化（可选，MemoryCache 无需特殊配置）
cache.initialize();
```

---

## API 方法

### 设置值

#### 无过期时间
```cpp
void set(const std::string& key, const std::string& value);
```

**例子**:
```cpp
cache.set("user:1:name", "Alice");
```

#### 带 TTL（生存时间）
```cpp
void set(const std::string& key, const std::string& value, std::chrono::seconds ttl);
```

**例子**:
```cpp
// 设置 24 小时过期
cache.set("session:token", "abc123def456", std::chrono::hours(24));

// 设置 1 小时过期
cache.set("cache:key", "value", std::chrono::hours(1));

// 设置 5 分钟过期
cache.set("temp:data", "value", std::chrono::minutes(5));

// 设置 30 秒过期
cache.set("short:lived", "value", std::chrono::seconds(30));
```

---

### 获取值

```cpp
std::optional<std::string> get(const std::string& key);
```

**返回值**:
- 如果 key 存在且未过期：返回值的 `std::optional`
- 如果 key 不存在或已过期：返回 `std::nullopt`

**例子**:
```cpp
auto value = cache.get("user:1:name");
if (value) {
    std::cout << "找到: " << *value << std::endl;
} else {
    std::cout << "不存在或已过期" << std::endl;
}

// 或使用 C++17 结构化绑定
if (auto val = cache.get("key"); val) {
    process(*val);
}
```

---

### 检查存在

```cpp
bool exists(const std::string& key);
```

**返回值**: true 如果 key 存在且未过期，否则 false

**例子**:
```cpp
if (cache.exists("user:session:123")) {
    std::cout << "会话有效" << std::endl;
} else {
    std::cout << "会话已过期或不存在" << std::endl;
}
```

---

### 删除值

```cpp
bool del(const std::string& key);
```

**返回值**: true 如果 key 存在且被删除，false 如果 key 不存在

**例子**:
```cpp
if (cache.del("user:session:123")) {
    std::cout << "会话已删除" << std::endl;
} else {
    std::cout << "会话不存在" << std::endl;
}
```

---

### 清空缓存

```cpp
void clear();
```

**例子**:
```cpp
// 清空所有缓存（谨慎使用！）
cache.clear();
```

---

### 获取缓存大小

```cpp
size_t size();
```

**返回值**: 缓存中的条目数（包括过期但未清理的条目）

**例子**:
```cpp
std::cout << "缓存中有 " << cache.size() << " 个条目" << std::endl;
```

---

### 健康检查

```cpp
bool ping();
```

**返回值**: 总是返回 true（为了与 RedisClient 兼容）

**例子**:
```cpp
if (cache.ping()) {
    std::cout << "缓存可用" << std::endl;
}
```

---

## 实际使用示例

### 用户会话管理（项目中使用）

```cpp
// 登录时缓存会话
void login(int64_t user_id, const std::string& token) {
    auto& cache = xpp::infrastructure::MemoryCache::instance();
    std::string cache_key = fmt::format("user:session:{}", user_id);
    
    // 设置 24 小时过期的会话
    cache.set(cache_key, token, std::chrono::hours(24));
    xpp::log_info("User {} logged in", user_id);
}

// 验证会话
std::optional<int64_t> verify_session(int64_t user_id, const std::string& token) {
    auto& cache = xpp::infrastructure::MemoryCache::instance();
    std::string cache_key = fmt::format("user:session:{}", user_id);
    
    auto cached_token = cache.get(cache_key);
    if (cached_token && *cached_token == token) {
        return user_id;  // 会话有效
    }
    return std::nullopt;  // 会话无效或过期
}

// 登出时删除会话
void logout(int64_t user_id) {
    auto& cache = xpp::infrastructure::MemoryCache::instance();
    std::string cache_key = fmt::format("user:session:{}", user_id);
    
    cache.del(cache_key);
    xpp::log_info("User {} logged out", user_id);
}
```

### 缓存用户数据

```cpp
// 缓存用户信息（5 分钟）
void cache_user(const User& user) {
    auto& cache = xpp::infrastructure::MemoryCache::instance();
    std::string key = fmt::format("user:{}", user.id);
    
    // 序列化用户为 JSON
    auto json = user.to_json().dump();
    cache.set(key, json, std::chrono::minutes(5));
}

// 获取缓存的用户信息
std::optional<User> get_cached_user(int64_t user_id) {
    auto& cache = xpp::infrastructure::MemoryCache::instance();
    std::string key = fmt::format("user:{}", user_id);
    
    auto json_str = cache.get(key);
    if (json_str) {
        return User::from_json(nlohmann::json::parse(*json_str));
    }
    return std::nullopt;
}
```

### 缓存 API 响应

```cpp
// 缓存昂贵操作的结果（10 分钟）
std::vector<Message> get_messages(int64_t user_id) {
    auto& cache = xpp::infrastructure::MemoryCache::instance();
    std::string cache_key = fmt::format("messages:{}", user_id);
    
    // 检查缓存
    if (auto cached = cache.get(cache_key); cached) {
        auto messages_json = nlohmann::json::parse(*cached);
        // 反序列化消息列表
        return deserialize_messages(messages_json);
    }
    
    // 缓存未命中，从数据库获取
    auto& db = infrastructure::DatabasePool::instance();
    auto messages = db.execute_sync(
        "SELECT * FROM messages WHERE user_id = ? ORDER BY created_at DESC LIMIT 100",
        user_id
    );
    
    // 缓存结果
    auto json = serialize_messages(messages).dump();
    cache.set(cache_key, json, std::chrono::minutes(10));
    
    return messages;
}
```

---

## 线程安全

MemoryCache 内部使用 `std::mutex` 保护所有操作，**完全线程安全**。

```cpp
// 多线程环境中安全使用
std::thread t1([&] {
    cache.set("key1", "value1", std::chrono::hours(1));
});

std::thread t2([&] {
    if (auto val = cache.get("key1"); val) {
        std::cout << *val << std::endl;
    }
});

t1.join();
t2.join();
```

---

## 自动过期

过期的条目会在以下情况下自动清理：
1. 调用 `get()` 时检查并删除过期条目
2. 其他访问时进行清理

**注意**: `size()` 返回的数字可能包含未被清理的过期条目。

---

## 与 RedisClient 的兼容性

MemoryCache 的 API 设计与 RedisClient 兼容，使迁移更容易：

| 操作 | MemoryCache | RedisClient | 兼容 |
|------|-----------|-------------|------|
| 设置值 | `set(k, v)` | `set(k, v)` | ✅ |
| 设置 TTL | `set(k, v, ttl)` | `set(k, v, ttl)` | ✅ |
| 获取值 | `get(k)` | `get(k)` | ✅ |
| 检查存在 | `exists(k)` | `exists(k)` | ✅ |
| 删除 | `del(k)` | `del(k)` | ✅ |
| 健康检查 | `ping()` | `ping()` | ✅ |

---

## 性能特性

- **获取**: O(1) 平均时间复杂度（哈希表查找）
- **设置**: O(1) 平均时间复杂度（哈希表插入）
- **删除**: O(1) 平均时间复杂度（哈希表删除）
- **存储**: 内存中（无磁盘 I/O）
- **并发**: 所有操作都互斥保护

---

## 注意事项

⚠️ **内存占用**: 所有数据存储在进程内存中
- 数据在服务器重启时丢失
- 不支持多进程共享

⚠️ **适用场景**:
- ✅ 开发和测试
- ✅ 单机部署
- ✅ 会话管理
- ✅ 短期数据缓存

✅ **何时切换到 Redis**:
- 需要跨重启持久化
- 分布式部署（多个服务器）
- 大规模应用

---

## 配置

MemoryCache 没有外部配置。初始化时传入空的 `Config` 结构：

```cpp
xpp::infrastructure::MemoryCache::Config config;
cache.initialize(config);

// 或更简单地
cache.initialize();
```

---

## 日志

MemoryCache 初始化时输出日志：

```cpp
XPP_LOG_INFO("Memory cache initialized (in-process, data will be lost on restart)");
```

所有操作（设置、获取、删除）都是静默的，除非发生错误。

---

## 完整示例

```cpp
#include "xpp/infrastructure/memory_cache.hpp"
#include "xpp/core/logger.hpp"

int main() {
    // 初始化
    auto& cache = xpp::infrastructure::MemoryCache::instance();
    cache.initialize();
    
    // 设置值
    cache.set("greeting", "Hello World");
    cache.set("temp_token", "abc123", std::chrono::seconds(30));
    
    // 获取值
    if (auto greeting = cache.get("greeting"); greeting) {
        xpp::log_info("Got greeting: {}", *greeting);
    }
    
    // 检查存在
    if (cache.exists("temp_token")) {
        xpp::log_info("Token exists");
    }
    
    // 删除值
    if (cache.del("temp_token")) {
        xpp::log_info("Token deleted");
    }
    
    // 获取大小
    xpp::log_info("Cache size: {}", cache.size());
    
    return 0;
}
```

---

更多详情请参考 `MIGRATION_SUMMARY.md` 和 `BUILD_GUIDE.md`。
