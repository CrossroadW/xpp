#include <gtest/gtest.h>
#include "xpp/infrastructure/database_pool.hpp"
#include <filesystem>
#include <fstream>

class DatabasePoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use a temporary database for testing
        test_db_path = "test_database.db";
        
        // Initialize database
        xpp::infrastructure::DatabasePool::Config config;
        config.database_file = test_db_path;
        config.auto_create = true;
        
        auto& pool = xpp::infrastructure::DatabasePool::instance();
        pool.initialize(config);
        
        // Create test table
        pool.execute_sync("DROP TABLE IF EXISTS test_table");
        pool.execute_sync(
            "CREATE TABLE test_table ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL,"
            "  value INTEGER NOT NULL"
            ")"
        );
    }

    void TearDown() override {
        // Clean up test database
        auto& pool = xpp::infrastructure::DatabasePool::instance();
        pool.execute_sync("DROP TABLE IF EXISTS test_table");
        
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
    }

    std::string test_db_path;
};

TEST_F(DatabasePoolTest, InsertData) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    auto result = pool.execute_sync(
        "INSERT INTO test_table (name, value) VALUES ('test1', 100)"
    );
    
    EXPECT_TRUE(result.is_success);
}

TEST_F(DatabasePoolTest, SelectData) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test1', 100)");
    
    auto result = pool.execute_sync("SELECT * FROM test_table WHERE name = 'test1'");
    
    EXPECT_FALSE(result.empty());
    EXPECT_GE(result[0].size(), 2);  // id, name, value
}

TEST_F(DatabasePoolTest, UpdateData) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test1', 100)");
    
    auto result = pool.execute_sync("UPDATE test_table SET value = 200 WHERE name = 'test1'");
    
    EXPECT_TRUE(result.is_success);
    
    auto check = pool.execute_sync("SELECT value FROM test_table WHERE name = 'test1'");
    EXPECT_FALSE(check.empty());
    EXPECT_EQ(check[0][0], "200");
}

TEST_F(DatabasePoolTest, DeleteData) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test1', 100)");
    
    auto result = pool.execute_sync("DELETE FROM test_table WHERE name = 'test1'");
    
    EXPECT_TRUE(result.is_success);
    
    auto check = pool.execute_sync("SELECT * FROM test_table WHERE name = 'test1'");
    EXPECT_TRUE(check.empty());
}

TEST_F(DatabasePoolTest, Transaction) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    {
        auto txn = pool.begin_transaction();
        
        pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('txn1', 100)");
        pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('txn2', 200)");
        
        txn.commit();
    }
    
    auto result = pool.execute_sync("SELECT COUNT(*) FROM test_table");
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result[0][0], "2");
}

TEST_F(DatabasePoolTest, GetLastInsertId) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test1', 100)");
    int64_t id1 = pool.last_insert_id();
    
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test2', 200)");
    int64_t id2 = pool.last_insert_id();
    
    EXPECT_GT(id2, id1);
}

TEST_F(DatabasePoolTest, EscapedStringInQuery) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    std::string name_with_quote = "test's name";
    std::string escaped = name_with_quote;
    // Simple SQL escaping: replace ' with ''
    size_t pos = 0;
    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "''");
        pos += 2;
    }
    
    auto result = pool.execute_sync(
        "INSERT INTO test_table (name, value) VALUES ('" + escaped + "', 100)"
    );
    
    EXPECT_TRUE(result.is_success);
}

TEST_F(DatabasePoolTest, EmptyResult) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    auto result = pool.execute_sync("SELECT * FROM test_table WHERE name = 'nonexistent'");
    
    EXPECT_TRUE(result.empty());
}

TEST_F(DatabasePoolTest, MultipleRows) {
    auto& pool = xpp::infrastructure::DatabasePool::instance();
    
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test1', 100)");
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test2', 200)");
    pool.execute_sync("INSERT INTO test_table (name, value) VALUES ('test3', 300)");
    
    auto result = pool.execute_sync("SELECT * FROM test_table");
    
    EXPECT_EQ(result.size(), 3);
}
