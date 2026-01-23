#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>

namespace xpp::modules::message {

struct Message {
    int64_t id;
    int64_t sender_id;
    int64_t receiver_id;
    std::string content;
    std::string message_type;
    bool is_read;
    int64_t created_at;

    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"sender_id", sender_id},
            {"receiver_id", receiver_id},
            {"content", content},
            {"message_type", message_type},
            {"is_read", is_read},
            {"created_at", created_at}
        };
    }
};

struct SendMessageRequest {
    int64_t receiver_id;
    std::string content;
    std::string message_type = "text";

    static SendMessageRequest from_json(const nlohmann::json& j) {
        return {
            .receiver_id = j.value("receiver_id", 0LL),
            .content = j.value("content", ""),
            .message_type = j.value("message_type", "text")
        };
    }

    bool is_valid() const {
        return receiver_id > 0 && !content.empty() && content.length() <= 10000;
    }
};

struct SendMessageResponse {
    Message message;

    nlohmann::json to_json() const {
        return {
            {"message", message.to_json()}
        };
    }
};

struct GetMessagesResponse {
    std::vector<Message> messages;

    nlohmann::json to_json() const {
        nlohmann::json messages_json = nlohmann::json::array();
        for (const auto& msg : messages) {
            messages_json.push_back(msg.to_json());
        }
        return {
            {"messages", messages_json}
        };
    }
};

} // namespace xpp::modules::message
