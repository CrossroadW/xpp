# Redis 替代方案指南

当前项目中 Redis 主要用于**用户会话缓存**。如果不想安装 Redis，有以下几种替代方案：

---

## 方案 1: 内存缓存 (In-Memory Cache) ⭐ **推荐用于开发**

### 优点
- ✅ 无需外部依赖
- ✅ 实现简单，适合开发和测试
- ✅ 速度快

### 缺点
- ❌ 进程重启数据丢失
- ❌ 不支持进程间共享
- ❌ 不适合生产环境多进程

### 实现步骤

**1. 创建内存缓存类** `include/xpp/infrastructure/memory_cache.hpp`

```cpp
#pragma once

#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <optional>
#include <chrono>

namespace xpp::infrastructure {

/**
 * @brief 线程安全的内存缓存
 */
class MemoryCache {
public:
    struct CacheEntry {
        std::string value;
        std::chrono::system_clock::time_point expiry;
    };

    static MemoryCache& instance() {
        static MemoryCache cache;
        return cache;
    }

    void initialize() {
        // 可选：启动过期数据清理线程
        xpp::log_info("Memory cache initialized");
    }

    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = {value, std::chrono::system_clock::time_point::max()};
    }

    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto expiry = std::chrono::system_clock::now() + ttl;
        cache_[key] = {value, expiry};
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        
        // 检查是否过期
        if (std::chrono::system_clock::now() > it->second.expiry) {
            cache_.erase(it);
            return std::nullopt;
        }
        
        return it->second.value;
    }

    bool exists(const std::string& key) {
        return get(key).has_value();
    }

    bool del(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.erase(key) > 0;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

private:
    MemoryCache() = default;
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex mutex_;
};

} // namespace xpp::infrastructure
```

**2. 修改 main.cpp 使用 MemoryCache**

```cpp
// 注释掉 Redis 初始化
/*
infrastructure::RedisClient::Config redis_config{...};
infrastructure::RedisClient::instance().initialize(redis_config);
*/

// 改用内存缓存
infrastructure::MemoryCache::instance().initialize();
```

**3. 修改 auth_service.hpp**

```cpp
// 将所有 RedisClient 改为 MemoryCache
// 从: auto& redis = infrastructure::RedisClient::instance();
// 改为: auto& cache = infrastructure::MemoryCache::instance();
```

---

## 方案 2: SQLite (轻量级数据库) ⭐ **推荐用于单机**

### 优点
- ✅ 单文件数据库，无需安装
- ✅ 数据持久化
- ✅ SQL 查询能力
- ✅ 适合单机应用

### 缺点
- ❌ 不支持高并发写入
- ❌ 不支持网络访问
- ❌ 不适合分布式

### 实现步骤

**1. 添加 SQLite 依赖到 conanfile.txt**

```
[requires]
sqlite3/3.44.0
```

**2. 创建 SQLite 缓存类** `include/xpp/infrastructure/sqlite_cache.hpp`

```cpp
#pragma once

#include <sqlite3.h>
#include <mutex>
#include <string>
#include <optional>
#include <chrono>

namespace xpp::infrastructure {

/**
 * @brief 基于 SQLite 的持久化缓存
 */
class SqliteCache {
public:
    static SqliteCache& instance() {
        static SqliteCache cache;
        return cache;
    }

    void initialize(const std::string& db_path = "cache.db") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite cache database");
        }

        // 创建缓存表
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS cache (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL,
                expiry INTEGER
            );
        )";
        
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string error = err ? err : "Unknown error";
            sqlite3_free(err);
            throw std::runtime_error("Failed to create cache table: " + error);
        }

        xpp::log_info("SQLite cache initialized at {}", db_path);
    }

    void set(const std::string& key, const std::string& value) {
        set_with_ttl(key, value, -1);
    }

    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
        set_with_ttl(key, value, ttl.count());
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 检查过期
        std::string sql = "DELETE FROM cache WHERE key = ? AND expiry > 0 AND expiry < ?";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }

        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, now);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        // 获取值
        sql = "SELECT value FROM cache WHERE key = ?";
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }

        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        
        std::optional<std::string> result;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        
        sqlite3_finalize(stmt);
        return result;
    }

    bool exists(const std::string& key) {
        return get(key).has_value();
    }

    bool del(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string sql = "DELETE FROM cache WHERE key = ?";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        int result = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        
        return result;
    }

    ~SqliteCache() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

private:
    SqliteCache() = default;

    void set_with_ttl(const std::string& key, const std::string& value, long long ttl_seconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        long long expiry = -1;
        if (ttl_seconds > 0) {
            expiry = std::chrono::system_clock::now().time_since_epoch().count() + ttl_seconds * 1000000000LL;
        }

        std::string sql = "INSERT OR REPLACE INTO cache (key, value, expiry) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return;
        }

        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, expiry);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;
};

} // namespace xpp::infrastructure
```

---

## 方案 3: PostgreSQL 会话表 ⭐ **推荐用于生产**

### 优点
- ✅ 充分利用现有 PostgreSQL
- ✅ 完全持久化
- ✅ 支持分布式
- ✅ 生产级别

### 缺点
- ❌ 性能不如 Redis
- ❌ 需要数据库查询

### 实现步骤

**1. 添加会话表到 init_db.sql**

```sql
CREATE TABLE user_sessions (
    id SERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token TEXT NOT NULL UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    INDEX idx_user_session(user_id),
    INDEX idx_token(token)
);

-- 清理过期会话
DELETE FROM user_sessions WHERE expires_at < CURRENT_TIMESTAMP;
```

**2. 创建 PostgreSQL 缓存类** `include/xpp/infrastructure/postgres_cache.hpp`

```cpp
#pragma once

#include "xpp/infrastructure/database_pool.hpp"
#include <string>
#include <optional>
#include <chrono>

namespace xpp::infrastructure {

/**
 * @brief 基于 PostgreSQL 的会话缓存
 */
class PostgresCache {
public:
    static PostgresCache& instance() {
        static PostgresCache cache;
        return cache;
    }

    void initialize() {
        xpp::log_info("PostgreSQL cache initialized");
    }

    void set(const std::string& key, const std::string& value) {
        auto ttl = std::chrono::hours(24);
        set_with_ttl(key, value, ttl);
    }

    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
        set_with_ttl(key, value, ttl);
    }

    std::optional<std::string> get(const std::string& key) {
        auto& db = DatabasePool::instance();
        
        try {
            auto result = db.execute_sync(
                "SELECT value FROM cache WHERE key = ? AND expires_at > NOW()",
                key
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            return std::string(result[0]["value"].as<std::string>());
        } catch (const std::exception& e) {
            xpp::log_error("Cache get error: {}", e.what());
            return std::nullopt;
        }
    }

    bool exists(const std::string& key) {
        return get(key).has_value();
    }

    bool del(const std::string& key) {
        auto& db = DatabasePool::instance();
        
        try {
            db.execute_sync("DELETE FROM cache WHERE key = ?", key);
            return true;
        } catch (const std::exception& e) {
            xpp::log_error("Cache delete error: {}", e.what());
            return false;
        }
    }

private:
    PostgresCache() = default;

    void set_with_ttl(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
        auto& db = DatabasePool::instance();
        
        try {
            db.execute_sync(
                "INSERT INTO cache (key, value, expires_at) VALUES (?, ?, NOW() + INTERVAL ? SECOND) "
                "ON CONFLICT(key) DO UPDATE SET value = ?, expires_at = NOW() + INTERVAL ? SECOND",
                key, value, ttl.count(), value, ttl.count()
            );
        } catch (const std::exception& e) {
            xpp::log_error("Cache set error: {}", e.what());
        }
    }
};

} // namespace xpp::infrastructure
```

---

## 方案 4: Memcached

### 优点
- ✅ 比 Redis 更轻量
- ✅ 专门为缓存优化

### 缺点
- ❌ 仍需安装额外服务
- ❌ 不如 Redis 功能丰富

---

## 快速切换方案

### 步骤：

1. **选择替代方案**（推荐方案 1: MemoryCache）

2. **修改 CMakeLists.txt**：移除 redis++ 依赖

3. **修改 conanfile.txt**：移除 redis++ 包

4. **修改 main.cpp**：
```cpp
// 注释 Redis 相关代码
// infrastructure::RedisClient::instance().initialize(redis_config);

// 改用选定方案
infrastructure::MemoryCache::instance().initialize();
// 或
infrastructure::SqliteCache::instance().initialize("cache.db");
// 或
infrastructure::PostgresCache::instance().initialize();
```

5. **修改 auth_service.hpp**：
```cpp
// 将所有 RedisClient 替换为选定的缓存类
auto& cache = infrastructure::MemoryCache::instance();
// auto& cache = infrastructure::SqliteCache::instance();
// auto& cache = infrastructure::PostgresCache::instance();
```

---

## 推荐方案总结

| 方案 | 场景 | 复杂度 | 性能 | 持久化 |
|------|------|-------|------|--------|
| MemoryCache | 开发/测试 | 低 | 很快 | ❌ |
| SQLite | 单机应用 | 中 | 快 | ✅ |
| PostgreSQL | 生产/分布式 | 中 | 一般 | ✅ |
| Memcached | 高性能缓存 | 高 | 非常快 | ❌ |

**建议**：
- 开发阶段用 **MemoryCache**
- 生产环境用 **PostgreSQL** 或 **Redis**
