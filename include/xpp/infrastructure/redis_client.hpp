#pragma once

#include <sw/redis++/redis++.h>
#include <memory>
#include <string>
#include <optional>
#include <chrono>
#include <vector>

namespace xpp::infrastructure {

using namespace sw::redis;

/**
 * @brief Redis client wrapper with connection pooling
 * Provides simplified interface for common Redis operations
 */
class RedisClient {
public:
    struct Config {
        std::string host = "localhost";
        int port = 6379;
        std::string password = "";
        int database = 0;
        size_t pool_size = 10;
        std::chrono::milliseconds connect_timeout{1000};
        std::chrono::milliseconds socket_timeout{1000};
    };

    static RedisClient& instance() {
        static RedisClient client;
        return client;
    }

    /**
     * @brief Initialize Redis connection
     */
    void initialize(const Config& config) {
        ConnectionOptions conn_opts;
        conn_opts.host = config.host;
        conn_opts.port = config.port;
        conn_opts.password = config.password;
        conn_opts.db = config.database;
        conn_opts.connect_timeout = config.connect_timeout;
        conn_opts.socket_timeout = config.socket_timeout;

        ConnectionPoolOptions pool_opts;
        pool_opts.size = config.pool_size;

        redis_ = std::make_unique<Redis>(conn_opts, pool_opts);

        xpp::log_info("Redis client initialized: {}:{}", config.host, config.port);
    }

    /**
     * @brief String operations
     */
    void set(const std::string& key, const std::string& value) {
        redis_->set(key, value);
    }

    void set(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
        redis_->set(key, value, ttl);
    }

    std::optional<std::string> get(const std::string& key) {
        auto result = redis_->get(key);
        if (result) {
            return std::optional<std::string>(*result);
        }
        return std::nullopt;
    }

    bool exists(const std::string& key) {
        return redis_->exists(key) > 0;
    }

    bool del(const std::string& key) {
        return redis_->del(key) > 0;
    }

    bool expire(const std::string& key, std::chrono::seconds ttl) {
        return redis_->expire(key, ttl);
    }

    /**
     * @brief Hash operations
     */
    void hset(const std::string& key, const std::string& field, const std::string& value) {
        redis_->hset(key, field, value);
    }

    std::optional<std::string> hget(const std::string& key, const std::string& field) {
        auto result = redis_->hget(key, field);
        if (result) {
            return std::optional<std::string>(*result);
        }
        return std::nullopt;
    }

    auto hgetall(const std::string& key) {
        std::unordered_map<std::string, std::string> result;
        redis_->hgetall(key, std::inserter(result, result.begin()));
        return result;
    }

    bool hdel(const std::string& key, const std::string& field) {
        return redis_->hdel(key, field) > 0;
    }

    bool hexists(const std::string& key, const std::string& field) {
        return redis_->hexists(key, field);
    }

    /**
     * @brief List operations
     */
    long long lpush(const std::string& key, const std::string& value) {
        return redis_->lpush(key, value);
    }

    long long rpush(const std::string& key, const std::string& value) {
        return redis_->rpush(key, value);
    }

    std::optional<std::string> lpop(const std::string& key) {
        auto result = redis_->lpop(key);
        if (result) {
            return std::optional<std::string>(*result);
        }
        return std::nullopt;
    }

    std::optional<std::string> rpop(const std::string& key) {
        auto result = redis_->rpop(key);
        if (result) {
            return std::optional<std::string>(*result);
        }
        return std::nullopt;
    }

    std::vector<std::string> lrange(const std::string& key, long long start, long long stop) {
        std::vector<std::string> result;
        redis_->lrange(key, start, stop, std::back_inserter(result));
        return result;
    }

    long long llen(const std::string& key) {
        return redis_->llen(key);
    }

    /**
     * @brief Set operations
     */
    long long sadd(const std::string& key, const std::string& member) {
        return redis_->sadd(key, member);
    }

    bool sismember(const std::string& key, const std::string& member) {
        return redis_->sismember(key, member);
    }

    std::vector<std::string> smembers(const std::string& key) {
        std::unordered_set<std::string> members;
        redis_->smembers(key, std::inserter(members, members.begin()));
        return std::vector<std::string>(members.begin(), members.end());
    }

    long long srem(const std::string& key, const std::string& member) {
        return redis_->srem(key, member);
    }

    /**
     * @brief Sorted set operations
     */
    long long zadd(const std::string& key, const std::string& member, double score) {
        return redis_->zadd(key, member, score);
    }

    std::vector<std::pair<std::string, double>> zrange_with_scores(
        const std::string& key, long long start, long long stop
    ) {
        std::vector<std::pair<std::string, double>> result;
        redis_->zrange(key, start, stop, std::back_inserter(result));
        return result;
    }

    std::optional<double> zscore(const std::string& key, const std::string& member) {
        auto result = redis_->zscore(key, member);
        if (result) {
            return std::optional<double>(*result);
        }
        return std::nullopt;
    }

    long long zrem(const std::string& key, const std::string& member) {
        return redis_->zrem(key, member);
    }

    /**
     * @brief Pub/Sub operations
     */
    long long publish(const std::string& channel, const std::string& message) {
        return redis_->publish(channel, message);
    }

    /**
     * @brief Key operations
     */
    std::vector<std::string> keys(const std::string& pattern) {
        std::vector<std::string> result;
        redis_->keys(pattern, std::back_inserter(result));
        return result;
    }

    long long ttl(const std::string& key) {
        return redis_->ttl(key);
    }

    /**
     * @brief Transaction support
     */
    class Transaction {
    public:
        explicit Transaction(Redis* redis) : pipeline_(redis->pipeline()) {}

        Transaction& set(const std::string& key, const std::string& value) {
            pipeline_.set(key, value);
            return *this;
        }

        Transaction& get(const std::string& key) {
            pipeline_.get(key);
            return *this;
        }

        // Add more operations as needed

        auto exec() {
            return pipeline_.exec();
        }

    private:
        Pipeline pipeline_;
    };

    Transaction transaction() {
        return Transaction(redis_.get());
    }

    /**
     * @brief Ping Redis server
     */
    bool ping() {
        try {
            redis_->ping();
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    /**
     * @brief Get underlying Redis object
     */
    Redis* redis() {
        return redis_.get();
    }

private:
    RedisClient() = default;
    ~RedisClient() = default;
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    std::unique_ptr<Redis> redis_;
};

} // namespace xpp::infrastructure
