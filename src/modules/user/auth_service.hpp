#pragma once

#include "user_model.hpp"
#include "xpp/infrastructure/database_pool.hpp"
#include "xpp/infrastructure/redis_client.hpp"
#include "xpp/core/logger.hpp"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <iomanip>
#include <sstream>
#include <random>
#include <optional>
#include <nlohmann/json.hpp>

namespace xpp::modules::user {

/**
 * @brief JWT token generator and validator
 */
class JwtService {
public:
    explicit JwtService(const std::string& secret) : secret_(secret) {}

    /**
     * @brief Generate JWT token
     */
    std::string generate(int64_t user_id, const std::string& username) {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto exp = now + hours(24);  // 24 hour expiration

        nlohmann::json header = {
            {"alg", "HS256"},
            {"typ", "JWT"}
        };

        nlohmann::json payload = {
            {"user_id", user_id},
            {"username", username},
            {"iat", duration_cast<seconds>(now.time_since_epoch()).count()},
            {"exp", duration_cast<seconds>(exp.time_since_epoch()).count()}
        };

        std::string header_encoded = base64_url_encode(header.dump());
        std::string payload_encoded = base64_url_encode(payload.dump());
        std::string message = header_encoded + "." + payload_encoded;
        std::string signature = hmac_sha256(message, secret_);

        return message + "." + base64_url_encode(signature);
    }

    /**
     * @brief Verify and decode JWT token
     */
    std::optional<nlohmann::json> verify(const std::string& token) {
        auto parts = split(token, '.');
        if (parts.size() != 3) {
            return std::nullopt;
        }

        std::string message = parts[0] + "." + parts[1];
        std::string signature = hmac_sha256(message, secret_);
        std::string expected_signature = base64_url_encode(signature);

        if (parts[2] != expected_signature) {
            return std::nullopt;
        }

        try {
            std::string payload_decoded = base64_url_decode(parts[1]);
            auto payload = nlohmann::json::parse(payload_decoded);

            // Check expiration
            auto now = std::chrono::system_clock::now();
            auto exp = payload.value("exp", 0LL);
            auto exp_time = std::chrono::system_clock::from_time_t(exp);

            if (now > exp_time) {
                return std::nullopt;
            }

            return payload;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    std::string secret_;

    std::string hmac_sha256(const std::string& message, const std::string& key) {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;

        ::HMAC(EVP_sha256(),
             key.c_str(), static_cast<int>(key.length()),
             reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
             hash, &hash_len);

        return std::string(reinterpret_cast<char*>(hash), hash_len);
    }

    std::string base64_url_encode(const std::string& input) {
        static const char* base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string ret;
        int val = 0;
        int valb = -6;

        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                ret.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }

        if (valb > -6) {
            ret.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        }

        // URL-safe encoding
        for (auto& c : ret) {
            if (c == '+') c = '-';
            if (c == '/') c = '_';
        }

        // Remove padding
        ret.erase(std::find(ret.begin(), ret.end(), '='), ret.end());

        return ret;
    }

    std::string base64_url_decode(const std::string& input) {
        std::string str = input;

        // Convert URL-safe back to standard
        for (auto& c : str) {
            if (c == '-') c = '+';
            if (c == '_') c = '/';
        }

        // Add padding
        while (str.length() % 4) {
            str += '=';
        }

        static const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string ret;
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

        int val = 0;
        int valb = -8;

        for (unsigned char c : str) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                ret.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }

        return ret;
    }

    std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream token_stream(s);
        while (std::getline(token_stream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }
};

/**
 * @brief Authentication service
 */
class AuthService {
public:
    AuthService()
        : jwt_service_("your-secret-key-change-this-in-production") {}

    /**
     * @brief Register a new user
     */
    std::optional<AuthResponse> register_user(const RegisterRequest& req) {
        if (!req.validate()) {
            xpp::log_warn("Invalid registration request");
            return std::nullopt;
        }

        auto& db = infrastructure::DatabasePool::instance();

        // Check if username exists
        auto result = db.execute_sync(
            "SELECT id FROM users WHERE username = $1",
            req.username
        );

        if (!result.empty()) {
            xpp::log_warn("Username already exists: {}", req.username);
            return std::nullopt;
        }

        // Hash password
        std::string password_hash = hash_password(req.password);

        // Insert user
        result = db.execute_sync(
            "INSERT INTO users (username, password_hash, email, created_at, updated_at) "
            "VALUES ($1, $2, $3, NOW(), NOW()) RETURNING id",
            req.username, password_hash, req.email
        );

        if (result.empty()) {
            xpp::log_error("Failed to create user");
            return std::nullopt;
        }

        int64_t user_id = result[0]["id"].as<int64_t>();

        User user{
            .id = user_id,
            .username = req.username,
            .email = req.email,
            .created_at = std::chrono::system_clock::now(),
            .updated_at = std::chrono::system_clock::now()
        };

        std::string token = jwt_service_.generate(user_id, user.username);

        // Cache user session in Redis
        cache_user_session(user_id, token);

        return AuthResponse{token, user};
    }

    /**
     * @brief Login user
     */
    std::optional<AuthResponse> login(const LoginRequest& req) {
        auto& db = infrastructure::DatabasePool::instance();

        auto result = db.execute_sync(
            "SELECT id, username, password_hash, email, avatar_url, is_active "
            "FROM users WHERE username = $1",
            req.username
        );

        if (result.empty()) {
            xpp::log_warn("User not found: {}", req.username);
            return std::nullopt;
        }

        auto row = result[0];
        std::string password_hash = row["password_hash"].as<std::string>();

        if (!verify_password(req.password, password_hash)) {
            xpp::log_warn("Invalid password for user: {}", req.username);
            return std::nullopt;
        }

        User user{
            .id = row["id"].as<int64_t>(),
            .username = row["username"].as<std::string>(),
            .email = row["email"].as<std::string>(),
            .avatar_url = row["avatar_url"].isNull() ? "" : row["avatar_url"].as<std::string>(),
            .is_active = row["is_active"].as<bool>()
        };

        std::string token = jwt_service_.generate(user.id, user.username);

        // Cache user session
        cache_user_session(user.id, token);

        xpp::log_info("User logged in: {}", user.username);

        return AuthResponse{token, user};
    }

    /**
     * @brief Verify token and get user info
     */
    std::optional<User> verify_token(const std::string& token) {
        auto payload = jwt_service_.verify(token);
        if (!payload) {
            return std::nullopt;
        }

        int64_t user_id = payload->value("user_id", 0LL);

        // Check Redis cache first
        auto& redis = infrastructure::RedisClient::instance();
        std::string cache_key = fmt::format("user:session:{}", user_id);

        if (redis.exists(cache_key)) {
            auto cached_token = redis.get(cache_key);
            if (cached_token && *cached_token == token) {
                return get_user_by_id(user_id);
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Logout user (invalidate token)
     */
    void logout(int64_t user_id) {
        auto& redis = infrastructure::RedisClient::instance();
        std::string cache_key = fmt::format("user:session:{}", user_id);
        redis.del(cache_key);
        xpp::log_info("User logged out: {}", user_id);
    }

private:
    JwtService jwt_service_;

    std::string hash_password(const std::string& password) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(password.c_str()),
               password.length(), hash);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

    bool verify_password(const std::string& password, const std::string& hash) {
        return hash_password(password) == hash;
    }

    void cache_user_session(int64_t user_id, const std::string& token) {
        auto& redis = infrastructure::RedisClient::instance();
        std::string cache_key = fmt::format("user:session:{}", user_id);
        redis.set(cache_key, token, std::chrono::hours(24));
    }

    std::optional<User> get_user_by_id(int64_t user_id) {
        auto& db = infrastructure::DatabasePool::instance();

        auto result = db.execute_sync(
            "SELECT id, username, email, avatar_url, is_active "
            "FROM users WHERE id = $1",
            user_id
        );

        if (result.empty()) {
            return std::nullopt;
        }

        auto row = result[0];
        return User{
            .id = row["id"].as<int64_t>(),
            .username = row["username"].as<std::string>(),
            .email = row["email"].as<std::string>(),
            .avatar_url = row["avatar_url"].isNull() ? "" : row["avatar_url"].as<std::string>(),
            .is_active = row["is_active"].as<bool>()
        };
    }
};

} // namespace xpp::modules::user
