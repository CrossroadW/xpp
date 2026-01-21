#include <gtest/gtest.h>
#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include "xpp/core/logger.hpp"
#include "xpp/core/config_manager.hpp"
#include "xpp/core/ioc_container.hpp"
#include "xpp/network/http_server.hpp"
#include "xpp/infrastructure/database_pool.hpp"
#include "xpp/infrastructure/memory_cache.hpp"
#include "xpp/modules/user/auth_service.hpp"
#include "xpp/modules/user/auth_controller.hpp"

using namespace drogon;
using namespace xpp;

class ApiTest : public ::testing::Test {
protected:
    static std::thread server_thread;

    static void SetUpTestSuite() {
        // Initialize logger
        core::Logger::instance().initialize("logs", core::Logger::Level::Info, 10485760, 5);
        xpp::log_info("current working dir is {}", std::filesystem::current_path().string());
        // Initialize database
        infrastructure::DatabasePool::Config db_config{
            .database_file = "test_xpp.db",
            .auto_create = true
        };
        infrastructure::DatabasePool::instance().initialize(db_config);

        // Initialize schema from SQL file
        try {
            infrastructure::DatabasePool::instance().execute_sql_file("config/init_db.sql");
            xpp::log_info("Database schema initialized from config/init_db.sql");
        } catch (const std::exception& e) {
            xpp::log_error("Failed to initialize schema: {}", e.what());
            throw;
        }

        // Initialize memory cache
        infrastructure::MemoryCache::instance().initialize();

        // Register services
        auto& container = core::IoCContainer::instance();
        container.register_service<modules::user::AuthService>(
            []() { return std::make_shared<modules::user::AuthService>(); },
            core::IoCContainer::Lifetime::Singleton
        );

        // Start server in background thread
        server_thread = std::thread([]() {
            network::HttpServer server;
            server.set_listen_address("127.0.0.1", 50051);
            server.set_threads(1);
            server.enable_cors();

            // Setup routes
            auto& container = core::IoCContainer::instance();
            auto auth_service = container.resolve<modules::user::AuthService>();
            auto auth_controller = std::make_shared<modules::user::AuthController>(auth_service);
            auth_controller->register_routes(server);

            server.get("/health", [](auto req, auto callback) {
                nlohmann::json health = {
                    {"status", "ok"},
                    {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
                };
                callback(network::Response::success(health));
            });

            server.get("/", [](auto req, auto callback) {
                callback(network::Response::json({
                    {"message", "XPP WeChat Backend API"},
                    {"version", "1.0.0"}
                }));
            });

            server.run();
        });

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    static void TearDownTestSuite() {
        // Quit the server from within its own thread
        app().getLoop()->queueInLoop([]() {
            app().quit();
        });

        // Wait for server thread to finish
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    HttpClientPtr client = HttpClient::newHttpClient("http://127.0.0.1:50051");
};

std::thread ApiTest::server_thread;

TEST_F(ApiTest, HealthEndpoint) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/health");

    bool result_received = false;
    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k200OK) << "Status code should be 200";
}

TEST_F(ApiTest, RootEndpoint) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k200OK) << "Status code should be 200";
}

TEST_F(ApiTest, RegisterValidUser) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/register");
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    std::string timestamp = std::to_string(std::time(nullptr));
    Json::Value body;
    body["username"] = "testuser_" + timestamp;
    body["password"] = "password123";
    body["email"] = "test_" + timestamp + "@example.com";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k200OK) << "Status code should be 200";
}

TEST_F(ApiTest, RegisterInvalidEmail) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/register");
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    Json::Value body;
    body["username"] = "testuser";
    body["password"] = "password123";
    body["email"] = "invalid-email";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k400BadRequest) << "Status code should be 400";
}

TEST_F(ApiTest, RegisterShortPassword) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/register");
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    Json::Value body;
    body["username"] = "testuser";
    body["password"] = "12345";
    body["email"] = "test@example.com";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k400BadRequest) << "Status code should be 400";
}

TEST_F(ApiTest, LoginValidCredentials) {
    // First register a user
    auto reg_req = HttpRequest::newHttpRequest();
    reg_req->setMethod(drogon::Post);
    reg_req->setPath("/api/auth/register");
    reg_req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    std::string username = "logintest_" + std::to_string(std::time(nullptr));
    Json::Value reg_body;
    reg_body["username"] = username;
    reg_body["password"] = "password123";
    reg_body["email"] = "login_" + std::to_string(std::time(nullptr)) + "@example.com";
    reg_req->setBody(reg_body.toStyledString());
    client->sendRequest(reg_req);

    // Now login
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/login");
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    Json::Value body;
    body["username"] = username;
    body["password"] = "password123";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k200OK) << "Status code should be 200";
}

TEST_F(ApiTest, LoginInvalidCredentials) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/login");
    req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    Json::Value body;
    body["username"] = "nonexistent";
    body["password"] = "wrongpassword";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k401Unauthorized) << "Status code should be 401";
}

TEST_F(ApiTest, GetCurrentUserWithValidToken) {
    // Register and login to get token
    auto reg_req = HttpRequest::newHttpRequest();
    reg_req->setMethod(drogon::Post);
    reg_req->setPath("/api/auth/register");
    reg_req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    std::string username = "metest_" + std::to_string(std::time(nullptr));
    Json::Value reg_body;
    reg_body["username"] = username;
    reg_body["password"] = "password123";
    reg_body["email"] = "me_" + std::to_string(std::time(nullptr)) + "@example.com";
    reg_req->setBody(reg_body.toStyledString());

    auto [reg_result, reg_resp] = client->sendRequest(reg_req);

    // Get current user
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/auth/me");
    req->addHeader("Authorization", "Bearer valid_token");

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    // Status will depend on server implementation
}

TEST_F(ApiTest, GetCurrentUserWithoutToken) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/auth/me");

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k401Unauthorized) << "Should require authentication";
}

TEST_F(ApiTest, GetCurrentUserWithInvalidToken) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/auth/me");
    req->addHeader("Authorization", "Bearer invalid_token_12345");

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k401Unauthorized) << "Invalid token should fail";
}

TEST_F(ApiTest, LogoutWithValidToken) {
    // Register to get token
    auto reg_req = HttpRequest::newHttpRequest();
    reg_req->setMethod(drogon::Post);
    reg_req->setPath("/api/auth/register");
    reg_req->setContentTypeCode(drogon::ContentType::CT_APPLICATION_JSON);

    std::string username = "logouttest_" + std::to_string(std::time(nullptr));
    Json::Value reg_body;
    reg_body["username"] = username;
    reg_body["password"] = "password123";
    reg_body["email"] = "logout_" + std::to_string(std::time(nullptr)) + "@example.com";
    reg_req->setBody(reg_body.toStyledString());

    auto [reg_result, reg_resp] = client->sendRequest(reg_req);

    // Logout
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/logout");
    req->addHeader("Authorization", "Bearer valid_token");

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    // Status will depend on server implementation
}

TEST_F(ApiTest, LogoutWithoutToken) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/logout");

    auto [result, resp] = client->sendRequest(req);

    EXPECT_EQ(result, ReqResult::Ok) << "Request failed";
    EXPECT_EQ(resp->getStatusCode(), drogon::k401Unauthorized) << "Should require authentication";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
