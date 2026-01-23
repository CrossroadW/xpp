#pragma once

#include "message_service.hpp"
#include "xpp/network/http_server.hpp"
#include "xpp/modules/user/auth_service.hpp"
#include "xpp/core/logger.hpp"
#include <memory>

namespace xpp::modules::message {

class MessageController {
public:
    explicit MessageController(std::shared_ptr<MessageService> service, std::shared_ptr<user::AuthService> auth_service)
        : service_(service), auth_service_(auth_service) {}

    void register_routes(network::HttpServer& server) {
        // Send message
        server.post("/api/messages/send", [this](auto req, auto callback) {
            handle_send_message(req, std::move(callback));
        });

        // Get inbox
        server.get("/api/messages/inbox", [this](auto req, auto callback) {
            handle_get_inbox(req, std::move(callback));
        });

        // Get sent messages
        server.get("/api/messages/sent", [this](auto req, auto callback) {
            handle_get_sent(req, std::move(callback));
        });

        // Get conversation with specific user
        server.get("/api/messages/conversation/{user_id}", [this](auto req, auto callback) {
            handle_get_conversation(req, std::move(callback));
        });

        // Mark message as read
        server.put("/api/messages/{message_id}/read", [this](auto req, auto callback) {
            handle_mark_as_read(req, std::move(callback));
        });
    }

private:
    std::optional<int64_t> authenticate_request(const drogon::HttpRequestPtr& req) {
        auto auth_header = req->getHeader("Authorization");
        if (auth_header.empty()) {
            return std::nullopt;
        }

        // Extract token from "Bearer <token>"
        if (auth_header.substr(0, 7) != "Bearer ") {
            return std::nullopt;
        }

        std::string token = auth_header.substr(7);
        auto user = auth_service_->verify_token(token);
        if (!user) {
            return std::nullopt;
        }

        return user->id;
    }

    void handle_send_message(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto user_id = authenticate_request(req);
        if (!user_id) {
            callback(network::Response::json({{"success", false}, {"error", "Unauthorized"}}, drogon::k401Unauthorized));
            return;
        }

        auto json = req->getJsonObject();
        if (!json) {
            callback(network::Response::json({{"success", false}, {"error", "Invalid JSON"}}, drogon::k400BadRequest));
            return;
        }

        nlohmann::json nlohmann_json = nlohmann::json::parse(json->toStyledString());
        auto message_req = SendMessageRequest::from_json(nlohmann_json);

        auto response = service_->send_message(*user_id, message_req);
        if (!response) {
            callback(network::Response::json({{"success", false}, {"error", "Failed to send message"}}, drogon::k400BadRequest));
            return;
        }

        callback(network::Response::success(response->to_json()));
    }

    void handle_get_inbox(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto user_id = authenticate_request(req);
        if (!user_id) {
            callback(network::Response::json({{"success", false}, {"error", "Unauthorized"}}, drogon::k401Unauthorized));
            return;
        }

        auto response = service_->get_inbox(*user_id);
        if (!response) {
            callback(network::Response::json({{"success", false}, {"error", "Failed to get inbox"}}, drogon::k500InternalServerError));
            return;
        }

        callback(network::Response::success(response->to_json()));
    }

    void handle_get_sent(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto user_id = authenticate_request(req);
        if (!user_id) {
            callback(network::Response::json({{"success", false}, {"error", "Unauthorized"}}, drogon::k401Unauthorized));
            return;
        }

        auto response = service_->get_sent(*user_id);
        if (!response) {
            callback(network::Response::json({{"success", false}, {"error", "Failed to get sent messages"}}, drogon::k500InternalServerError));
            return;
        }

        callback(network::Response::success(response->to_json()));
    }

    void handle_get_conversation(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto user_id = authenticate_request(req);
        if (!user_id) {
            callback(network::Response::json({{"success", false}, {"error", "Unauthorized"}}, drogon::k401Unauthorized));
            return;
        }

        auto other_user_id_str = req->getParameter("user_id");
        if (other_user_id_str.empty()) {
            callback(network::Response::json({{"success", false}, {"error", "Missing user_id parameter"}}, drogon::k400BadRequest));
            return;
        }

        int64_t other_user_id = std::stoll(other_user_id_str);
        auto response = service_->get_conversation(*user_id, other_user_id);
        if (!response) {
            callback(network::Response::json({{"success", false}, {"error", "Failed to get conversation"}}, drogon::k500InternalServerError));
            return;
        }

        callback(network::Response::success(response->to_json()));
    }

    void handle_mark_as_read(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto user_id = authenticate_request(req);
        if (!user_id) {
            callback(network::Response::json({{"success", false}, {"error", "Unauthorized"}}, drogon::k401Unauthorized));
            return;
        }

        auto message_id_str = req->getParameter("message_id");
        if (message_id_str.empty()) {
            callback(network::Response::json({{"success", false}, {"error", "Missing message_id parameter"}}, drogon::k400BadRequest));
            return;
        }

        int64_t message_id = std::stoll(message_id_str);
        bool success = service_->mark_as_read(message_id, *user_id);
        if (!success) {
            callback(network::Response::json({{"success", false}, {"error", "Failed to mark message as read"}}, drogon::k400BadRequest));
            return;
        }

        callback(network::Response::success({{"message", "Message marked as read"}}));
    }

    std::shared_ptr<MessageService> service_;
    std::shared_ptr<user::AuthService> auth_service_;
};

} // namespace xpp::modules::message
