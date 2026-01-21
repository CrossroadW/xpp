#include <gtest/gtest.h>
#include "xpp/modules/user/auth_service.hpp"
#include "xpp/infrastructure/database_pool.hpp"
#include <filesystem>
#include "xpp/modules/user/auth_service.hpp"

class AuthServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path = "auth_test_database.db";
        
        // Initialize database
        xpp::infrastructure::DatabasePool::Config config;
        config.database_file = test_db_path;
        config.auto_create = true;
        
        auto& pool = xpp::infrastructure::DatabasePool::instance();
        pool.initialize(config);
        
        // Create users table
        pool.execute_sync("DROP TABLE IF EXISTS users");
        pool.execute_sync(
            "CREATE TABLE users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  username TEXT UNIQUE NOT NULL,"
            "  password_hash TEXT NOT NULL,"
            "  email TEXT UNIQUE NOT NULL,"
            "  avatar_url TEXT,"
            "  is_active BOOLEAN DEFAULT 1,"
            "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
            ")"
        );
        
        // Initialize auth service
        auth_service = std::make_unique<xpp::modules::user::AuthService>();
    }

    void TearDown() override {
        auto& pool = xpp::infrastructure::DatabasePool::instance();
        pool.execute_sync("DROP TABLE IF EXISTS users");
        
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
    }

    std::string test_db_path;
    std::unique_ptr<xpp::modules::user::AuthService> auth_service;
};

TEST_F(AuthServiceTest, RegisterNewUser) {
    xpp::modules::user::RegisterRequest req{
        .username = "testuser",
        .password = "password123",
        .email = "test@example.com"
    };
    
    auto response = auth_service->register_user(req);
    
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->user.username, "testuser");
    EXPECT_EQ(response->user.email, "test@example.com");
    EXPECT_FALSE(response->token.empty());
}

TEST_F(AuthServiceTest, RegisterDuplicateUsername) {
    xpp::modules::user::RegisterRequest req1{
        .username = "testuser",
        .password = "password123",
        .email = "test1@example.com"
    };
    
    xpp::modules::user::RegisterRequest req2{
        .username = "testuser",
        .password = "password456",
        .email = "test2@example.com"
    };
    
    auto response1 = auth_service->register_user(req1);
    EXPECT_TRUE(response1.has_value());
    
    auto response2 = auth_service->register_user(req2);
    EXPECT_FALSE(response2.has_value());
}

TEST_F(AuthServiceTest, LoginValidCredentials) {
    // Register user first
    xpp::modules::user::RegisterRequest reg_req{
        .username = "testuser",
        .password = "password123",
        .email = "test@example.com"
    };
    
    auto reg_response = auth_service->register_user(reg_req);
    ASSERT_TRUE(reg_response.has_value());
    
    // Login with correct credentials
    xpp::modules::user::LoginRequest login_req{
        .username = "testuser",
        .password = "password123"
    };
    
    auto login_response = auth_service->login(login_req);
    
    ASSERT_TRUE(login_response.has_value());
    EXPECT_EQ(login_response->user.username, "testuser");
    EXPECT_FALSE(login_response->token.empty());
}

TEST_F(AuthServiceTest, LoginInvalidPassword) {
    // Register user first
    xpp::modules::user::RegisterRequest reg_req{
        .username = "testuser",
        .password = "password123",
        .email = "test@example.com"
    };
    
    auth_service->register_user(reg_req);
    
    // Login with wrong password
    xpp::modules::user::LoginRequest login_req{
        .username = "testuser",
        .password = "wrongpassword"
    };
    
    auto login_response = auth_service->login(login_req);
    
    EXPECT_FALSE(login_response.has_value());
}

TEST_F(AuthServiceTest, LoginNonexistentUser) {
    xpp::modules::user::LoginRequest login_req{
        .username = "nonexistent",
        .password = "password123"
    };
    
    auto login_response = auth_service->login(login_req);
    
    EXPECT_FALSE(login_response.has_value());
}

TEST_F(AuthServiceTest, LogoutUser) {
    // Register and login
    xpp::modules::user::RegisterRequest reg_req{
        .username = "testuser",
        .password = "password123",
        .email = "test@example.com"
    };
    
    auto reg_response = auth_service->register_user(reg_req);
    ASSERT_TRUE(reg_response.has_value());
    
    int64_t user_id = reg_response->user.id;
    
    // Logout should not throw
    EXPECT_NO_THROW(auth_service->logout(user_id));
}

TEST_F(AuthServiceTest, JWTTokenValidation) {
    // Register user
    xpp::modules::user::RegisterRequest reg_req{
        .username = "testuser",
        .password = "password123",
        .email = "test@example.com"
    };
    
    auto reg_response = auth_service->register_user(reg_req);
    ASSERT_TRUE(reg_response.has_value());
    
    const auto& token = reg_response->token;
    const auto& user = reg_response->user;
    
    // Verify token
    auto verified_user = auth_service->verify_token(token);
    
    ASSERT_TRUE(verified_user.has_value());
    EXPECT_EQ(verified_user->id, user.id);
    EXPECT_EQ(verified_user->username, user.username);
}

TEST_F(AuthServiceTest, InvalidToken) {
    auto verified_user = auth_service->verify_token("invalid.token.here");
    
    EXPECT_FALSE(verified_user.has_value());
}

TEST_F(AuthServiceTest, MultipleUserRegistration) {
    for (int i = 0; i < 5; ++i) {
        xpp::modules::user::RegisterRequest req{
            .username = "user" + std::to_string(i),
            .password = "password123",
            .email = "user" + std::to_string(i) + "@example.com"
        };
        
        auto response = auth_service->register_user(req);
        EXPECT_TRUE(response.has_value());
    }
}
