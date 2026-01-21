// 此文件包含 auth_service.hpp 的 SQLite3 兼容版本
// 请将下面的代码手动复制覆盖原 src/modules/user/auth_service.hpp

#pragma once

#include "user_model.hpp"
#include "xpp/infrastructure/database_pool.hpp"
#include "xpp/infrastructure/memory_cache.hpp"
#include "xpp/core/logger.hpp"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <iomanip>
#include <sstream>
#include <random>
#include <optional>
#include <nlohmann/json.hpp>
#include <fmt/core.h>

namespace xpp::modules::user {

/**
 * @brief JWT token generator and validator
 */
class JwtService {
public:
    explicit JwtService(const std::string& secret) : secret_(secret) {}

    std::string generate(int64_t user_id, const std::string& username) {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto exp = now + hours(24);

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

    std::optional<nlohmann::json> verify(const std::string& token) {
        auto parts = split(token, '.');
        if (parts.size() != 3) return std::nullopt;

        std::string message = parts[0] + "." + parts[1];
        std::string signature = hmac_sha256(message, secret_);
        std::string expected_signature = base64_url_encode(signature);

        if (parts[2] != expected_signature) return std::nullopt;

        try {
            auto payload = nlohmann::json::parse(base64_url_decode(parts[1]));
            auto now = std::chrono::system_clock::now();
            auto exp = payload.value("exp", 0LL);
            auto exp_time = std::chrono::system_clock::from_time_t(exp);
            if (now > exp_time) return std::nullopt;
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
        ::HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
             reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
             hash, &hash_len);
        return std::string(reinterpret_cast<char*>(hash), hash_len);
    }

    std::string base64_url_encode(const std::string& input) {
        static const char* base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string ret;
        int val = 0, valb = -6;
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                ret.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) ret.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        for (auto& c : ret) {
            if (c == '+') c = '-';
            if (c == '/') c = '_';
        }
        ret.erase(std::find(ret.begin(), ret.end(), '='), ret.end());
        return ret;
    }

    std::string base64_url_decode(const std::string& input) {
        std::string str = input;
        for (auto& c : str) {
            if (c == '-') c = '+';
            if (c == '_') c = '/';
        }
        while (str.length() % 4) str += '=';

        static const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string ret;
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

        int val = 0, valb = -8;
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
        while (std::getline(token_stream, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }
};

inline std::string escape_sql_string(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        if (c == '\'') escaped += "''";
        else escaped += c;
    }
    return escaped;
}

class AuthService {
public:
    AuthService() : jwt_service_("your-secret-key-change-this-in-production") {}

    std::optional<AuthResponse> register_user(const RegisterRequest& req) {
        if (!req.validate()) {
            xpp::log_warn("Invalid registration request");
            return std::nullopt;
        }

        auto& db = infrastructure::DatabasePool::instance();
        std::string username_escaped = escape_sql_string(req.username);
        
        auto result = db.execute_sync(
            fmt::format("SELECT id FROM users WHERE username = '{}'", username_escaped)
        );

        if (!result.empty()) {
            xpp::log_warn("Username already exists: {}", req.username);
            return std::nullopt;
        }

        std::string password_hash = hash_password(req.password);
        std::string email_escaped = escape_sql_string(req.email);

        result = db.execute_sync(
            fmt::format("INSERT INTO users (username, password_hash, email, created_at, updated_at) "
                "VALUES ('{}', '{}', '{}', datetime('now'), datetime('now'))",
                username_escaped, password_hash, email_escaped)
        );

        if (!result.is_success) {
            xpp::log_error("Failed to create user: {}", result.error_message);
            return std::nullopt;
        }

        int64_t user_id = db.last_insert_id();
        User user {
            .id = user_id,
            .username = req.username,
            .email = req.email,
            .created_at = std::chrono::system_clock::now(),
            .updated_at = std::chrono::system_clock::now()
        };

        std::string token = jwt_service_.generate(user_id, user.username);
        cache_user_session(user_id, token);
        return AuthResponse{token, user};
    }

    std::optional<AuthResponse> login(const LoginRequest& req) {
        auto& db = infrastructure::DatabasePool::instance();
        std::string username_escaped = escape_sql_string(req.username);
        
        auto result = db.execute_sync(
            fmt::format("SELECT id, username, password_hash, email, avatar_url, is_active "
                "FROM users WHERE username = '{}'", username_escaped)
        );

        if (result.empty()) {
            xpp::log_warn("User not found: {}", req.username);
            return std::nullopt;
        }

        auto row = result[0];
        if (!verify_password(req.password, row[2])) {
            xpp::log_warn("Invalid password for user: {}", req.username);
            return std::nullopt;
        }

        User user {
            .id = std::stoll(row[0]),
            .username = row[1],
            .email = row[3],
            .avatar_url = row[4],
            .is_active = row[5] == "1"
        };

        std::string token = jwt_service_.generate(user.id, user.username);
        cache_user_session(user.id, token);
        xpp::log_info("User logged in: {}", user.username);
        return AuthResponse{token, user};
    }

    std::optional<User> verify_token(const std::string& token) {
        auto payload = jwt_service_.verify(token);
        if (!payload) return std::nullopt;

        int64_t user_id = payload->value("user_id", 0LL);
        auto& cache = infrastructure::MemoryCache::instance();
        std::string cache_key = fmt::format("user:session:{}", user_id);

        if (cache.exists(cache_key)) {
            auto cached_token = cache.get(cache_key);
            if (cached_token && *cached_token == token)
                return get_user_by_id(user_id);
        }
        return std::nullopt;
    }

    void logout(int64_t user_id) {
        auto& cache = infrastructure::MemoryCache::instance();
        cache.del(fmt::format("user:session:{}", user_id));
        xpp::log_info("User logged out: {}", user_id);
    }

private:
    JwtService jwt_service_;

    std::string hash_password(const std::string& password) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(password.c_str()),
               password.length(), hash);
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        return ss.str();
    }

    bool verify_password(const std::string& password, const std::string& hash) {
        return hash_password(password) == hash;
    }

    void cache_user_session(int64_t user_id, const std::string& token) {
        auto& cache = infrastructure::MemoryCache::instance();
        cache.set(fmt::format("user:session:{}", user_id), token, std::chrono::hours(24));
    }

    std::optional<User> get_user_by_id(int64_t user_id) {
        auto& db = infrastructure::DatabasePool::instance();
        auto result = db.execute_sync(
            fmt::format("SELECT id, username, email, avatar_url, is_active FROM users WHERE id = {}", user_id)
        );
        if (result.empty()) return std::nullopt;
        
        auto row = result[0];
        return User {
            .id = std::stoll(row[0]),
            .username = row[1],
            .email = row[2],
            .avatar_url = row[3],
            .is_active = row[4] == "1"
        };
    }
};

} // namespace xpp::modules::user
