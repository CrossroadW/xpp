#include <gtest/gtest.h>
#include "xpp/modules/message/message_service.hpp"
#include "xpp/modules/user/auth_service.hpp"
#include "xpp/infrastructure/database_pool.hpp"
#include "xpp/infrastructure/memory_cache.hpp"
#include <filesystem>

class MessageServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_db_path = "message_test_database.db";

        // Initialize database
        xpp::infrastructure::DatabasePool::Config config;
        config.database_file = test_db_path;
        config.auto_create = true;

        auto& pool = xpp::infrastructure::DatabasePool::instance();
        pool.initialize(config);

        // Create tables
        pool.execute_sync("DROP TABLE IF EXISTS messages");
        pool.execute_sync("DROP TABLE IF EXISTS users");

        pool.execute_sync(
            "CREATE TABLE users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  username TEXT UNIQUE NOT NULL,"
            "  password_hash TEXT NOT NULL,"
            "  email TEXT UNIQUE NOT NULL,"
            "  avatar_url TEXT,"
            "  is_active INTEGER DEFAULT 1,"
            "  created_at INTEGER DEFAULT (strftime('%s', 'now')),"
            "  updated_at INTEGER DEFAULT (strftime('%s', 'now'))"
            ")"
        );

        pool.execute_sync(
            "CREATE TABLE messages ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  sender_id INTEGER NOT NULL,"
            "  receiver_id INTEGER NOT NULL,"
            "  content TEXT NOT NULL,"
            "  message_type TEXT DEFAULT 'text',"
            "  is_read INTEGER DEFAULT 0,"
            "  created_at INTEGER DEFAULT (strftime('%s', 'now'))"
            ")"
        );

        // Initialize memory cache
        xpp::infrastructure::MemoryCache::instance().initialize();

        // Create test users
        auth_service = std::make_unique<xpp::modules::user::AuthService>();

        xpp::modules::user::RegisterRequest user1_req{
            .username = "user1",
            .password = "password123",
            .email = "user1@test.com"
        };
        auto user1_resp = auth_service->register_user(user1_req);
        user1_id = user1_resp->user.id;

        xpp::modules::user::RegisterRequest user2_req{
            .username = "user2",
            .password = "password123",
            .email = "user2@test.com"
        };
        auto user2_resp = auth_service->register_user(user2_req);
        user2_id = user2_resp->user.id;

        // Initialize message service
        message_service = std::make_unique<xpp::modules::message::MessageService>();
    }

    void TearDown() override {
        auto& pool = xpp::infrastructure::DatabasePool::instance();
        pool.close();

        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }
    }

    std::string test_db_path;
    std::unique_ptr<xpp::modules::user::AuthService> auth_service;
    std::unique_ptr<xpp::modules::message::MessageService> message_service;
    int64_t user1_id;
    int64_t user2_id;
};

TEST_F(MessageServiceTest, SendMessage) {
    xpp::modules::message::SendMessageRequest req{
        .receiver_id = user2_id,
        .content = "Hello, user2!",
        .message_type = "text"
    };

    auto response = message_service->send_message(user1_id, req);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->message.sender_id, user1_id);
    EXPECT_EQ(response->message.receiver_id, user2_id);
    EXPECT_EQ(response->message.content, "Hello, user2!");
    EXPECT_FALSE(response->message.is_read);
}

TEST_F(MessageServiceTest, SendMessageToInvalidUser) {
    xpp::modules::message::SendMessageRequest req{
        .receiver_id = 99999,
        .content = "Hello!",
        .message_type = "text"
    };

    auto response = message_service->send_message(user1_id, req);

    EXPECT_FALSE(response.has_value());
}

TEST_F(MessageServiceTest, SendEmptyMessage) {
    xpp::modules::message::SendMessageRequest req{
        .receiver_id = user2_id,
        .content = "",
        .message_type = "text"
    };

    auto response = message_service->send_message(user1_id, req);

    EXPECT_FALSE(response.has_value());
}

TEST_F(MessageServiceTest, GetInbox) {
    // Send messages from user1 to user2
    xpp::modules::message::SendMessageRequest req1{
        .receiver_id = user2_id,
        .content = "Message 1",
        .message_type = "text"
    };
    message_service->send_message(user1_id, req1);

    xpp::modules::message::SendMessageRequest req2{
        .receiver_id = user2_id,
        .content = "Message 2",
        .message_type = "text"
    };
    message_service->send_message(user1_id, req2);

    // Get user2's inbox
    auto response = message_service->get_inbox(user2_id);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->messages.size(), 2);
    EXPECT_EQ(response->messages[0].content, "Message 2"); // Most recent first
    EXPECT_EQ(response->messages[1].content, "Message 1");
}

TEST_F(MessageServiceTest, GetSent) {
    // Send messages from user1 to user2
    xpp::modules::message::SendMessageRequest req1{
        .receiver_id = user2_id,
        .content = "Message 1",
        .message_type = "text"
    };
    message_service->send_message(user1_id, req1);

    xpp::modules::message::SendMessageRequest req2{
        .receiver_id = user2_id,
        .content = "Message 2",
        .message_type = "text"
    };
    message_service->send_message(user1_id, req2);

    // Get user1's sent messages
    auto response = message_service->get_sent(user1_id);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->messages.size(), 2);
}

TEST_F(MessageServiceTest, GetConversation) {
    // Send messages between user1 and user2
    xpp::modules::message::SendMessageRequest req1{
        .receiver_id = user2_id,
        .content = "Hello from user1",
        .message_type = "text"
    };
    message_service->send_message(user1_id, req1);

    xpp::modules::message::SendMessageRequest req2{
        .receiver_id = user1_id,
        .content = "Hello from user2",
        .message_type = "text"
    };
    message_service->send_message(user2_id, req2);

    // Get conversation from user1's perspective
    auto response = message_service->get_conversation(user1_id, user2_id);

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->messages.size(), 2);
}

TEST_F(MessageServiceTest, MarkAsRead) {
    // Send message from user1 to user2
    xpp::modules::message::SendMessageRequest req{
        .receiver_id = user2_id,
        .content = "Test message",
        .message_type = "text"
    };
    auto send_response = message_service->send_message(user1_id, req);
    ASSERT_TRUE(send_response.has_value());

    int64_t message_id = send_response->message.id;

    // Mark as read by user2
    bool success = message_service->mark_as_read(message_id, user2_id);
    EXPECT_TRUE(success);

    // Verify it's marked as read
    auto inbox = message_service->get_inbox(user2_id);
    ASSERT_TRUE(inbox.has_value());
    ASSERT_FALSE(inbox->messages.empty());
    EXPECT_TRUE(inbox->messages[0].is_read);
}

TEST_F(MessageServiceTest, MarkAsReadWrongUser) {
    // Send message from user1 to user2
    xpp::modules::message::SendMessageRequest req{
        .receiver_id = user2_id,
        .content = "Test message",
        .message_type = "text"
    };
    auto send_response = message_service->send_message(user1_id, req);
    ASSERT_TRUE(send_response.has_value());

    int64_t message_id = send_response->message.id;

    // Try to mark as read by user1 (sender, not receiver)
    bool success = message_service->mark_as_read(message_id, user1_id);
    EXPECT_FALSE(success);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
