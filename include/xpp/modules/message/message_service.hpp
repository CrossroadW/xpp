#pragma once

#include "message_model.hpp"
#include "xpp/infrastructure/database_pool.hpp"
#include "xpp/core/logger.hpp"
#include <optional>
#include <chrono>
#include <fmt/core.h>

namespace xpp::modules::message {

class MessageService {
public:
    MessageService() = default;

    std::optional<SendMessageResponse> send_message(int64_t sender_id, const SendMessageRequest& req) {
        if (!req.is_valid()) {
            xpp::log_warn("Invalid message request from user {}", sender_id);
            return std::nullopt;
        }

        auto& db = infrastructure::DatabasePool::instance();

        // Verify receiver exists
        auto receiver_check = db.execute_sync(
            fmt::format("SELECT id FROM users WHERE id = {} AND is_active = 1", req.receiver_id)
        );
        if (receiver_check.empty()) {
            xpp::log_warn("Receiver {} not found or inactive", req.receiver_id);
            return std::nullopt;
        }

        // Escape content for SQL
        std::string escaped_content;
        for (char c : req.content) {
            if (c == '\'') escaped_content += "''";
            else escaped_content += c;
        }

        // Insert message
        auto now = std::chrono::system_clock::now().time_since_epoch().count() / 1000000000;
        auto insert_result = db.execute_sync(fmt::format(
            "INSERT INTO messages (sender_id, receiver_id, content, message_type, is_read, created_at) "
            "VALUES ({}, {}, '{}', '{}', 0, {})",
            sender_id, req.receiver_id, escaped_content, req.message_type, now
        ));

        if (!insert_result.is_success) {
            xpp::log_error("Failed to insert message: {}", insert_result.error_message);
            return std::nullopt;
        }

        int64_t message_id = db.last_insert_id();

        Message message{
            .id = message_id,
            .sender_id = sender_id,
            .receiver_id = req.receiver_id,
            .content = req.content,
            .message_type = req.message_type,
            .is_read = false,
            .created_at = now
        };

        xpp::log_info("Message {} sent from {} to {}", message_id, sender_id, req.receiver_id);

        return SendMessageResponse{.message = message};
    }

    std::optional<GetMessagesResponse> get_inbox(int64_t user_id, int limit = 50) {
        auto& db = infrastructure::DatabasePool::instance();

        auto result = db.execute_sync(fmt::format(
            "SELECT id, sender_id, receiver_id, content, message_type, is_read, created_at "
            "FROM messages WHERE receiver_id = {} ORDER BY created_at DESC LIMIT {}",
            user_id, limit
        ));

        std::vector<Message> messages;
        for (const auto& row : result.rows) {
            messages.push_back({
                .id = std::stoll(row[0]),
                .sender_id = std::stoll(row[1]),
                .receiver_id = std::stoll(row[2]),
                .content = row[3],
                .message_type = row[4],
                .is_read = row[5] == "1",
                .created_at = std::stoll(row[6])
            });
        }

        return GetMessagesResponse{.messages = messages};
    }

    std::optional<GetMessagesResponse> get_sent(int64_t user_id, int limit = 50) {
        auto& db = infrastructure::DatabasePool::instance();

        auto result = db.execute_sync(fmt::format(
            "SELECT id, sender_id, receiver_id, content, message_type, is_read, created_at "
            "FROM messages WHERE sender_id = {} ORDER BY created_at DESC LIMIT {}",
            user_id, limit
        ));

        std::vector<Message> messages;
        for (const auto& row : result.rows) {
            messages.push_back({
                .id = std::stoll(row[0]),
                .sender_id = std::stoll(row[1]),
                .receiver_id = std::stoll(row[2]),
                .content = row[3],
                .message_type = row[4],
                .is_read = row[5] == "1",
                .created_at = std::stoll(row[6])
            });
        }

        return GetMessagesResponse{.messages = messages};
    }

    std::optional<GetMessagesResponse> get_conversation(int64_t user_id, int64_t other_user_id, int limit = 50) {
        auto& db = infrastructure::DatabasePool::instance();

        auto result = db.execute_sync(fmt::format(
            "SELECT id, sender_id, receiver_id, content, message_type, is_read, created_at "
            "FROM messages WHERE (sender_id = {} AND receiver_id = {}) OR (sender_id = {} AND receiver_id = {}) "
            "ORDER BY created_at DESC LIMIT {}",
            user_id, other_user_id, other_user_id, user_id, limit
        ));

        std::vector<Message> messages;
        for (const auto& row : result.rows) {
            messages.push_back({
                .id = std::stoll(row[0]),
                .sender_id = std::stoll(row[1]),
                .receiver_id = std::stoll(row[2]),
                .content = row[3],
                .message_type = row[4],
                .is_read = row[5] == "1",
                .created_at = std::stoll(row[6])
            });
        }

        return GetMessagesResponse{.messages = messages};
    }

    bool mark_as_read(int64_t message_id, int64_t user_id) {
        auto& db = infrastructure::DatabasePool::instance();

        // Verify the message belongs to this user (as receiver)
        auto check = db.execute_sync(fmt::format(
            "SELECT id FROM messages WHERE id = {} AND receiver_id = {}",
            message_id, user_id
        ));

        if (check.empty()) {
            xpp::log_warn("Message {} not found or not owned by user {}", message_id, user_id);
            return false;
        }

        auto result = db.execute_sync(fmt::format(
            "UPDATE messages SET is_read = 1 WHERE id = {}",
            message_id
        ));

        if (result.is_success) {
            xpp::log_info("Message {} marked as read by user {}", message_id, user_id);
            return true;
        }

        return false;
    }
};

} // namespace xpp::modules::message
