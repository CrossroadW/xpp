#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <nlohmann/json.hpp>

namespace xpp::modules::user {

/**
 * @brief User entity
 */
struct User {
    int64_t id = 0;
    std::string username;
    std::string password_hash;  // Never send to client
    std::string email;
    std::string avatar_url;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
    bool is_active = true;

    // Convert to JSON (without password)
    nlohmann::json to_json() const {
        return {
            {"id", id},
            {"username", username},
            {"email", email},
            {"avatar_url", avatar_url},
            {"is_active", is_active},
            {"created_at", std::chrono::duration_cast<std::chrono::seconds>(
                created_at.time_since_epoch()
            ).count()}
        };
    }
};

/**
 * @brief User login credentials
 */
struct LoginRequest {
    std::string username;
    std::string password;

    static LoginRequest from_json(const nlohmann::json& j) {
        return {
            .username = j.value("username", ""),
            .password = j.value("password", "")
        };
    }
};

/**
 * @brief User registration data
 */
struct RegisterRequest {
    std::string username;
    std::string password;
    std::string email;

    static RegisterRequest from_json(const nlohmann::json& j) {
        return {
            .username = j.value("username", ""),
            .password = j.value("password", ""),
            .email = j.value("email", "")
        };
    }

    bool validate() const {
        return !username.empty() &&
               !password.empty() &&
               password.length() >= 6 &&
               !email.empty() &&
               email.find('@') != std::string::npos;
    }
};

/**
 * @brief Authentication response
 */
struct AuthResponse {
    std::string token;
    User user;

    nlohmann::json to_json() const {
        return {
            {"token", token},
            {"user", user.to_json()}
        };
    }
};

} // namespace xpp::modules::user
