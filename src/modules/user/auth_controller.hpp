#pragma once

#include "auth_service.hpp"
#include "xpp/network/http_server.hpp"
#include "xpp/core/logger.hpp"

namespace xpp::modules::user {

using namespace xpp::network;

/**
 * @brief Authentication HTTP controller
 */
class AuthController {
public:
    explicit AuthController(std::shared_ptr<AuthService> auth_service)
        : auth_service_(auth_service) {}

    /**
     * @brief Register routes
     */
    void register_routes(HttpServer& server) {
        server.post("/api/auth/register", [this](auto req, auto callback) {
            handle_register(req, std::move(callback));
        });

        server.post("/api/auth/login", [this](auto req, auto callback) {
            handle_login(req, std::move(callback));
        });

        server.post("/api/auth/logout", [this](auto req, auto callback) {
            handle_logout(req, std::move(callback));
        });

        server.get("/api/auth/me", [this](auto req, auto callback) {
            handle_get_current_user(req, std::move(callback));
        });
    }

private:
    std::shared_ptr<AuthService> auth_service_;

    /**
     * @brief POST /api/auth/register
     */
    void handle_register(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    ) {
        try {
            auto json = req->getJsonObject();
            if (!json) {
                callback(Response::error("Invalid JSON", k400BadRequest));
                return;
            }

            // Convert Json::Value (jsoncpp) to nlohmann::json
            nlohmann::json nlohmann_json = nlohmann::json::parse(json->toStyledString());
            auto register_req = RegisterRequest::from_json(nlohmann_json);
            auto result = auth_service_->register_user(register_req);

            if (!result) {
                callback(Response::error("Registration failed", k400BadRequest));
                return;
            }

            xpp::log_info("User registered: {}", register_req.username);
            callback(Response::success(result->to_json()));

        } catch (const std::exception& e) {
            xpp::log_error("Register error: {}", e.what());
            callback(Response::error("Internal server error", k500InternalServerError));
        }
    }

    /**
     * @brief POST /api/auth/login
     */
    void handle_login(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    ) {
        try {
            auto json = req->getJsonObject();
            if (!json) {
                callback(Response::error("Invalid JSON", k400BadRequest));
                return;
            }

            // Convert Json::Value (jsoncpp) to nlohmann::json
            nlohmann::json nlohmann_json = nlohmann::json::parse(json->toStyledString());
            auto login_req = LoginRequest::from_json(nlohmann_json);
            auto result = auth_service_->login(login_req);

            if (!result) {
                callback(Response::error("Invalid credentials", k401Unauthorized));
                return;
            }

            callback(Response::success(result->to_json()));

        } catch (const std::exception& e) {
            xpp::log_error("Login error: {}", e.what());
            callback(Response::error("Internal server error", k500InternalServerError));
        }
    }

    /**
     * @brief POST /api/auth/logout
     */
    void handle_logout(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    ) {
        try {
            auto user_id_opt = extract_user_id_from_token(req);
            if (!user_id_opt) {
                callback(Response::error("Unauthorized", k401Unauthorized));
                return;
            }

            auth_service_->logout(*user_id_opt);
            callback(Response::success());

        } catch (const std::exception& e) {
            xpp::log_error("Logout error: {}", e.what());
            callback(Response::error("Internal server error", k500InternalServerError));
        }
    }

    /**
     * @brief GET /api/auth/me
     */
    void handle_get_current_user(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback
    ) {
        try {
            auto token = extract_token(req);
            if (!token) {
                callback(Response::error("Missing authorization token", k401Unauthorized));
                return;
            }

            auto user = auth_service_->verify_token(*token);
            if (!user) {
                callback(Response::error("Invalid or expired token", k401Unauthorized));
                return;
            }

            callback(Response::success(user->to_json()));

        } catch (const std::exception& e) {
            xpp::log_error("Get current user error: {}", e.what());
            callback(Response::error("Internal server error", k500InternalServerError));
        }
    }

    /**
     * @brief Extract JWT token from Authorization header
     */
    std::optional<std::string> extract_token(const HttpRequestPtr& req) {
        auto auth_header = req->getHeader("Authorization");
        if (auth_header.empty()) {
            return std::nullopt;
        }

        // Format: "Bearer <token>"
        if (auth_header.substr(0, 7) == "Bearer ") {
            return auth_header.substr(7);
        }

        return std::nullopt;
    }

    std::optional<int64_t> extract_user_id_from_token(const HttpRequestPtr& req) {
        auto token = extract_token(req);
        if (!token) {
            return std::nullopt;
        }

        auto user = auth_service_->verify_token(*token);
        if (!user) {
            return std::nullopt;
        }

        return user->id;
    }
};

} // namespace xpp::modules::user
