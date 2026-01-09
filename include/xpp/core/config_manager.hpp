#pragma once

#include <string>
#include <memory>
#include <optional>
#include <stdexcept>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <mutex>

namespace xpp::core {

/**
 * @brief Unified configuration manager supporting YAML and JSON
 * Thread-safe singleton for accessing application configuration
 */
class ConfigManager {
public:
    using Json = nlohmann::json;

    static ConfigManager& instance() {
        static ConfigManager manager;
        return manager;
    }

    /**
     * @brief Load configuration from YAML file
     */
    void load_yaml(const std::string& file_path) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("Config file not found: " + file_path);
        }

        try {
            YAML::Node yaml = YAML::LoadFile(file_path);
            config_ = yaml_to_json(yaml);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Failed to load YAML config: ") + e.what()
            );
        }
    }

    /**
     * @brief Load configuration from JSON file
     */
    void load_json(const std::string& file_path) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!std::filesystem::exists(file_path)) {
            throw std::runtime_error("Config file not found: " + file_path);
        }

        try {
            std::ifstream file(file_path);
            config_ = Json::parse(file);
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Failed to load JSON config: ") + e.what()
            );
        }
    }

    /**
     * @brief Get configuration value by path (dot notation)
     * @example get<int>("server.port")
     */
    template<typename T>
    std::optional<T> get(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            auto value = get_value_by_path(path);
            if (value.is_null()) {
                return std::nullopt;
            }
            return value.get<T>();
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Get configuration value with default
     */
    template<typename T>
    T get_or(const std::string& path, const T& default_value) const {
        auto value = get<T>(path);
        return value.value_or(default_value);
    }

    /**
     * @brief Set configuration value
     */
    template<typename T>
    void set(const std::string& path, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        set_value_by_path(path, value);
    }

    /**
     * @brief Check if path exists
     */
    bool has(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            auto value = get_value_by_path(path);
            return !value.is_null();
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Get entire configuration as JSON
     */
    Json get_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    /**
     * @brief Clear all configuration
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = Json::object();
    }

    /**
     * @brief Save configuration to JSON file
     */
    void save_json(const std::string& file_path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream file(file_path);
        file << config_.dump(4);
    }

private:
    ConfigManager() : config_(Json::object()) {}
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Convert YAML node to JSON
    Json yaml_to_json(const YAML::Node& node) {
        switch (node.Type()) {
            case YAML::NodeType::Null:
                return nullptr;

            case YAML::NodeType::Scalar: {
                try {
                    return Json(node.as<int>());
                } catch (...) {}
                try {
                    return Json(node.as<double>());
                } catch (...) {}
                try {
                    return Json(node.as<bool>());
                } catch (...) {}
                return Json(node.as<std::string>());
            }

            case YAML::NodeType::Sequence: {
                Json array = Json::array();
                for (const auto& item : node) {
                    array.push_back(yaml_to_json(item));
                }
                return array;
            }

            case YAML::NodeType::Map: {
                Json object = Json::object();
                for (const auto& pair : node) {
                    object[pair.first.as<std::string>()] = yaml_to_json(pair.second);
                }
                return object;
            }

            default:
                return nullptr;
        }
    }

    // Get value by dot-separated path
    Json get_value_by_path(const std::string& path) const {
        auto keys = split_path(path);
        Json current = config_;

        for (const auto& key : keys) {
            if (!current.is_object() || !current.contains(key)) {
                return Json();
            }
            current = current[key];
        }

        return current;
    }

    // Set value by dot-separated path
    template<typename T>
    void set_value_by_path(const std::string& path, const T& value) {
        auto keys = split_path(path);
        Json* current = &config_;

        for (size_t i = 0; i < keys.size() - 1; ++i) {
            const auto& key = keys[i];
            if (!current->is_object()) {
                *current = Json::object();
            }
            if (!current->contains(key)) {
                (*current)[key] = Json::object();
            }
            current = &(*current)[key];
        }

        if (!current->is_object()) {
            *current = Json::object();
        }
        (*current)[keys.back()] = value;
    }

    // Split path by dots
    std::vector<std::string> split_path(const std::string& path) const {
        std::vector<std::string> result;
        std::string current;

        for (char ch : path) {
            if (ch == '.') {
                if (!current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
            } else {
                current += ch;
            }
        }

        if (!current.empty()) {
            result.push_back(current);
        }

        return result;
    }

    mutable std::mutex mutex_;
    Json config_;
};

} // namespace xpp::core
