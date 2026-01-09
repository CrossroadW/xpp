#include "xpp/core/logger.hpp"
#include "xpp/core/config_manager.hpp"
#include "xpp/core/ioc_container.hpp"
#include "xpp/core/event_bus.hpp"
#include "xpp/network/http_server.hpp"
#include "xpp/infrastructure/database_pool.hpp"
#include "xpp/infrastructure/redis_client.hpp"
#include "modules/user/auth_service.hpp"
#include "modules/user/auth_controller.hpp"
#include <iostream>
#include <csignal>

using namespace xpp;

// Global server instance for signal handling
network::HttpServer* g_server = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        xpp::log_info("Received shutdown signal, stopping server...");
        if (g_server) {
            g_server->stop();
        }
    }
}

/**
 * @brief Initialize all core services
 */
void initialize_services() {
    auto& config = core::ConfigManager::instance();

    // Initialize logger
    auto log_level_str = config.get_or<std::string>("logging.level", "info");
    auto log_level = core::Logger::Level::Info;

    if (log_level_str == "trace") log_level = core::Logger::Level::Trace;
    else if (log_level_str == "debug") log_level = core::Logger::Level::Debug;
    else if (log_level_str == "warn") log_level = core::Logger::Level::Warn;
    else if (log_level_str == "error") log_level = core::Logger::Level::Error;

    core::Logger::instance().initialize(
        config.get_or<std::string>("logging.log_dir", "logs"),
        log_level,
        config.get_or<size_t>("logging.max_file_size", 10485760),
        config.get_or<size_t>("logging.max_files", 5)
    );

    xpp::log_info("=== XPP WeChat Backend Starting ===");

    // Initialize database
    infrastructure::DatabasePool::Config db_config{
        .host = config.get_or<std::string>("database.host", "localhost"),
        .port = static_cast<uint16_t>(config.get_or<int>("database.port", 5432)),
        .database = config.get_or<std::string>("database.database", "xpp_db"),
        .username = config.get_or<std::string>("database.username", "postgres"),
        .password = config.get_or<std::string>("database.password", ""),
        .connection_num = config.get_or<size_t>("database.connection_num", 10)
    };

    try {
        infrastructure::DatabasePool::instance().initialize(db_config);
    } catch (const std::exception& e) {
        xpp::log_error("Failed to initialize database: {}", e.what());
        throw;
    }

    // Initialize Redis
    infrastructure::RedisClient::Config redis_config{
        .host = config.get_or<std::string>("redis.host", "localhost"),
        .port = config.get_or<int>("redis.port", 6379),
        .password = config.get_or<std::string>("redis.password", ""),
        .database = config.get_or<int>("redis.database", 0),
        .pool_size = config.get_or<size_t>("redis.pool_size", 10)
    };

    try {
        infrastructure::RedisClient::instance().initialize(redis_config);
        if (!infrastructure::RedisClient::instance().ping()) {
            xpp::log_warn("Redis connection test failed");
        }
    } catch (const std::exception& e) {
        xpp::log_error("Failed to initialize Redis: {}", e.what());
        throw;
    }

    xpp::log_info("All services initialized successfully");
}

/**
 * @brief Register all application modules
 */
void register_modules() {
    auto& container = core::IoCContainer::instance();

    // Register services
    container.register_service<modules::user::AuthService>(
        []() { return std::make_shared<modules::user::AuthService>(); },
        core::IoCContainer::Lifetime::Singleton
    );

    xpp::log_info("Modules registered");
}

/**
 * @brief Setup HTTP routes
 */
void setup_routes(network::HttpServer& server) {
    auto& container = core::IoCContainer::instance();
    auto auth_service = container.resolve<modules::user::AuthService>();

    // Auth routes
    auto auth_controller = std::make_shared<modules::user::AuthController>(auth_service);
    auth_controller->register_routes(server);

    // Health check endpoint
    server.get("/health", [](auto req, auto callback) {
        nlohmann::json health = {
            {"status", "ok"},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };
        callback(network::Response::success(health));
    });

    // Root endpoint
    server.get("/", [](auto req, auto callback) {
        callback(network::Response::json({
            {"message", "XPP WeChat Backend API"},
            {"version", "1.0.0"}
        }));
    });

    xpp::log_info("Routes registered");
}

int main() {
    try {
        // Load configuration
        auto& config = core::ConfigManager::instance();

        std::string config_file = "config/config.yaml";
        if (std::filesystem::exists(config_file)) {
            config.load_yaml(config_file);
            xpp::log_info("Configuration loaded from {}", config_file);
        } else {
            xpp::log_warn("Config file not found, using defaults");
        }

        // Initialize services
        initialize_services();

        // Register modules
        register_modules();

        // Create HTTP server
        network::HttpServer server;

        server.set_listen_address(
            config.get_or<std::string>("server.host", "0.0.0.0"),
            static_cast<uint16_t>(config.get_or<int>("server.port", 8080))
        );

        server.set_threads(config.get_or<size_t>("server.threads", 4));

        if (config.get_or<bool>("server.enable_cors", true)) {
            server.enable_cors();
        }

        // Setup routes
        setup_routes(server);

        // Register signal handlers
        g_server = &server;
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        // Start server
        xpp::log_info("Server starting on {}:{}",
                 config.get_or<std::string>("server.host", "0.0.0.0"),
                 config.get_or<int>("server.port", 8080));

        server.run();

        xpp::log_info("Server stopped");

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
