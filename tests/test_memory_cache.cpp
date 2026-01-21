#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "xpp/infrastructure/memory_cache.hpp"

class MemoryCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = &xpp::infrastructure::MemoryCache::instance();
        cache->clear();
    }

    void TearDown() override {
        cache->clear();
    }

    xpp::infrastructure::MemoryCache* cache;
};

TEST_F(MemoryCacheTest, SetAndGetValue) {
    cache->set("key1", "value1");
    auto result = cache->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");
}

TEST_F(MemoryCacheTest, GetNonexistentKey) {
    auto result = cache->get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MemoryCacheTest, ExistsKey) {
    cache->set("exists", "value");
    EXPECT_TRUE(cache->exists("exists"));
    EXPECT_FALSE(cache->exists("notexists"));
}

TEST_F(MemoryCacheTest, DeleteKey) {
    cache->set("todelete", "value");
    EXPECT_TRUE(cache->exists("todelete"));
    cache->del("todelete");
    EXPECT_FALSE(cache->exists("todelete"));
}

TEST_F(MemoryCacheTest, ClearAllKeys) {
    cache->set("key1", "value1");
    cache->set("key2", "value2");
    cache->set("key3", "value3");
    EXPECT_EQ(cache->size(), 3);
    cache->clear();
    EXPECT_EQ(cache->size(), 0);
}

TEST_F(MemoryCacheTest, SizeAfterOperations) {
    EXPECT_EQ(cache->size(), 0);
    cache->set("key1", "value1");
    EXPECT_EQ(cache->size(), 1);
    cache->set("key2", "value2");
    EXPECT_EQ(cache->size(), 2);
    cache->del("key1");
    EXPECT_EQ(cache->size(), 1);
}

TEST_F(MemoryCacheTest, OverwriteExistingKey) {
    cache->set("key", "value1");
    EXPECT_EQ(cache->get("key").value(), "value1");
    cache->set("key", "value2");
    EXPECT_EQ(cache->get("key").value(), "value2");
}

TEST_F(MemoryCacheTest, Ping) {
    EXPECT_EQ(cache->ping(), "PONG");
}

TEST_F(MemoryCacheTest, TTLExpiration) {
    using namespace std::chrono;
    cache->set("expiring", "value", milliseconds(100));
    EXPECT_TRUE(cache->exists("expiring"));
    
    std::this_thread::sleep_for(milliseconds(150));
    EXPECT_FALSE(cache->exists("expiring"));
}

TEST_F(MemoryCacheTest, ThreadSafety) {
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int operations_per_thread = 100;

    // Set values from multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                cache->set(key, "value_" + std::to_string(j));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(cache->size(), num_threads * operations_per_thread);
}

TEST_F(MemoryCacheTest, VariousValueTypes) {
    cache->set("string", "hello");
    cache->set("number", "42");
    cache->set("float", "3.14");
    
    EXPECT_EQ(cache->get("string").value(), "hello");
    EXPECT_EQ(cache->get("number").value(), "42");
    EXPECT_EQ(cache->get("float").value(), "3.14");
}
