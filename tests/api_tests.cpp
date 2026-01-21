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
    static void SetUpTestSuite() {
        // Initialize logger
        core::Logger::instance().initialize("logs", core::Logger::Level::Info, 10485760, 5);

        // Initialize database
        infrastructure::DatabasePool::Config db_config{
            .database = "test_xpp.db",
            .auto_create = true
        };
        infrastructure::DatabasePool::instance().initialize(db_config);

        // Initialize memory cache
        infrastructure::MemoryCache::instance().initialize();

        // Register services
        auto& container = core::IoCContainer::instance();
        container.register_service<modules::user::AuthService>(
            []() { return std::make_shared<modules::user::AuthService>(); },
            core::IoCContainer::Lifetime::Singleton
        );

        // Start server in background thread
        std::thread([]() {
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
        }).detach();

        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    static void TearDownTestSuite() {
        app().quit();
    }

    HttpClientPtr client = HttpClient::newHttpClient("http://127.0.0.1:50051");
};

TEST_F(ApiTest, HealthEndpoint) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/health");

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k200OK);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_TRUE((*json)["success"].asBool());
    EXPECT_EQ((*json)["data"]["status"].asString(), "ok");
    EXPECT_TRUE((*json)["data"]["timestamp"].isInt64());
}

TEST_F(ApiTest, RootEndpoint) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k200OK);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_EQ((*json)["message"].asString(), "XPP WeChat Backend API");
    EXPECT_EQ((*json)["version"].asString(), "1.0.0");
}

TEST_F(ApiTest, RegisterValidUser) {
    auto req = HttpRequest::newHttpJsonRequest(Json::Value());
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/register");

    Json::Value body;
    body["username"] = "testuser_" + std::to_string(std::time(nullptr));
    body["password"] = "password123";
    body["email"] = "test@example.com";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k200OK);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_TRUE((*json)["success"].asBool());
    EXPECT_FALSE((*json)["data"]["token"].asString().empty());
    EXPECT_EQ((*json)["data"]["user"]["username"].asString(), body["username"].asString());
    EXPECT_EQ((*json)["data"]["user"]["email"].asString(), "test@example.com");
    EXPECT_TRUE((*json)["data"]["user"]["is_active"].asBool());
}

TEST_F(ApiTest, RegisterInvalidEmail) {
    auto req = HttpRequest::newHttpJsonRequest(Json::Value());
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/register");

    Json::Value body;
    body["username"] = "testuser";
    body["password"] = "password123";
    body["email"] = "invalid-email";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k400BadRequest);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_FALSE((*json)["success"].asBool());
    EXPECT_FALSE((*json)["error"].asString().empty());
}

TEST_F(ApiTest, RegisterShortPassword) {
    auto req = HttpRequest::newHttpJsonRequest(Json::Value());
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/register");

    Json::Value body;
    body["username"] = "testuser";
    body["password"] = "12345";
    body["email"] = "test@example.com";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k400BadRequest);
}

TEST_F(ApiTest, LoginValidCredentials) {
    // First register a user
    auto reg_req = HttpRequest::newHttpJsonRequest(Json::Value());
    reg_req->setMethod(drogon::Post);
    reg_req->setPath("/api/auth/register");

    std::string username = "logintest_" + std::to_string(std::time(nullptr));
    Json::Value reg_body;
    reg_body["username"] = username;
    reg_body["password"] = "password123";
    reg_body["email"] = "login@example.com";
    reg_req->setBody(reg_body.toStyledString());
    client->sendRequest(reg_req);

    // Now login
    auto req = HttpRequest::newHttpJsonRequest(Json::Value());
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/login");

    Json::Value body;
    body["username"] = username;
    body["password"] = "password123";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k200OK);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_TRUE((*json)["success"].asBool());
    EXPECT_FALSE((*json)["data"]["token"].asString().empty());
    EXPECT_EQ((*json)["data"]["user"]["username"].asString(), username);
}

TEST_F(ApiTest, LoginInvalidCredentials) {
    auto req = HttpRequest::newHttpJsonRequest(Json::Value());
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/login");

    Json::Value body;
    body["username"] = "nonexistent";
    body["password"] = "wrongpassword";
    req->setBody(body.toStyledString());

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k401Unauthorized);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_FALSE((*json)["success"].asBool());
}

TEST_F(ApiTest, GetCurrentUserWithValidToken) {
    // Register and login to get token
    auto reg_req = HttpRequest::newHttpJsonRequest(Json::Value());
    reg_req->setMethod(drogon::Post);
    reg_req->setPath("/api/auth/register");

    std::string username = "metest_" + std::to_string(std::time(nullptr));
    Json::Value reg_body;
    reg_body["username"] = username;
    reg_body["password"] = "password123";
    reg_body["email"] = "me@example.com";
    reg_req->setBody(reg_body.toStyledString());

    auto [reg_result, reg_resp] = client->sendRequest(reg_req);
    auto reg_json = reg_resp->getJsonObject();
    std::string token = (*reg_json)["data"]["token"].asString();

    // Get current user
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/auth/me");
    req->addHeader("Authorization", "Bearer " + token);

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k200OK);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_TRUE((*json)["success"].asBool());
    EXPECT_EQ((*json)["data"]["username"].asString(), username);
    EXPECT_EQ((*json)["data"]["email"].asString(), "me@example.com");
}

TEST_F(ApiTest, GetCurrentUserWithoutToken) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/auth/me");

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k401Unauthorized);
}

TEST_F(ApiTest, GetCurrentUserWithInvalidToken) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/api/auth/me");
    req->addHeader("Authorization", "Bearer invalid_token_12345");

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k401Unauthorized);
}

TEST_F(ApiTest, LogoutWithValidToken) {
    // Register to get token
    auto reg_req = HttpRequest::newHttpJsonRequest(Json::Value());
    reg_req->setMethod(drogon::Post);
    reg_req->setPath("/api/auth/register");

    std::string username = "logouttest_" + std::to_string(std::time(nullptr));
    Json::Value reg_body;
    reg_body["username"] = username;
    reg_body["password"] = "password123";
    reg_body["email"] = "logout@example.com";
    reg_req->setBody(reg_body.toStyledString());

    auto [reg_result, reg_resp] = client->sendRequest(reg_req);
    auto reg_json = reg_resp->getJsonObject();
    std::string token = (*reg_json)["data"]["token"].asString();

    // Logout
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/logout");
    req->addHeader("Authorization", "Bearer " + token);

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k200OK);

    auto json = resp->getJsonObject();
    ASSERT_TRUE(json);
    EXPECT_TRUE((*json)["success"].asBool());
    EXPECT_EQ((*json)["data"]["message"].asString(), "Logged out successfully");
}

TEST_F(ApiTest, LogoutWithoutToken) {
    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/api/auth/logout");

    auto [result, resp] = client->sendRequest(req);

    ASSERT_EQ(result, ReqResult::Ok);
    ASSERT_EQ(resp->getStatusCode(), k401Unauthorized);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
