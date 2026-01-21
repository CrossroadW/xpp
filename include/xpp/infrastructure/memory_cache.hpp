#pragma once

#include "xpp/core/logger.hpp"
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <optional>
#include <chrono>
#include <limits>

// Prevent Windows macro pollution
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace xpp::infrastructure {

/**
 * @brief Thread-safe in-memory cache with TTL support
 * Singleton pattern - use MemoryCache::instance() to access
 * Ideal for development, testing, and single-process deployments
 */
class MemoryCache {
public:
    struct CacheEntry {
        std::string value;
        std::chrono::system_clock::time_point expiry;
    };

    struct Config {
        // Placeholder for future configuration options
        // Currently no external config needed for memory cache
    };

    static MemoryCache& instance() {
        static MemoryCache cache;
        return cache;
    }

    /**
     * @brief Initialize memory cache
     * @param config Configuration (currently unused, for API compatibility)
     */
    void initialize(const Config& config = Config{}) {
        xpp::log_info("Memory cache initialized (in-process, data will be lost on restart)");
    }

    /**
     * @brief Set a value without expiration
     * @param key Cache key
     * @param value Value to store
     */
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto max_time = std::chrono::system_clock::time_point::max();
        cache_[key] = {value, max_time};
    }

    /**
     * @brief Set a value with TTL (time to live). Accepts any std::chrono duration
     * @param key Cache key
     * @param value Value to store
     * @param ttl Time until expiration (any std::chrono duration)
     */
    template <typename Rep, typename Period>
    void set(const std::string& key, const std::string& value, std::chrono::duration<Rep, Period> ttl) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto expiry = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(ttl);
        cache_[key] = {value, expiry};
    }

    /**
     * @brief Get a value from cache
     * Automatically removes expired entries
     * @param key Cache key
     * @return Value if exists and not expired, std::nullopt otherwise
     */
    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        
        // Check if expired
        if (std::chrono::system_clock::now() > it->second.expiry) {
            cache_.erase(it);
            return std::nullopt;
        }
        
        return it->second.value;
    }

    /**
     * @brief Check if key exists in cache (and not expired)
     * @param key Cache key
     * @return true if key exists and not expired
     */
    bool exists(const std::string& key) {
        return get(key).has_value();
    }

    /**
     * @brief Delete a key from cache
     * @param key Cache key
     * @return true if key existed and was deleted
     */
    bool del(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.erase(key) > 0;
    }

    /**
     * @brief Clear all entries from cache
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
    }

    /**
     * @brief Get current cache size
     * Note: Includes expired entries (they're cleaned up on access)
     */
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

    /**
     * @brief Ping the cache (for compatibility with RedisClient)
     * Return 'PONG' string for compatibility tests
     */
    std::string ping() {
        return "PONG";
    }

private:
    MemoryCache() = default;
    ~MemoryCache() = default;
    
    // Prevent copying
    MemoryCache(const MemoryCache&) = delete;
    MemoryCache& operator=(const MemoryCache&) = delete;

    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex mutex_;
};

} // namespace xpp::infrastructure
