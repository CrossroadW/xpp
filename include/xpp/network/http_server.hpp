#pragma once

#include <drogon/drogon.h>
#include <drogon/WebSocketController.h>
#include <functional>
#include <memory>
#include <string>
#include <atomic>

namespace xpp::network {

using namespace drogon;

/**
 * @brief HTTP Server wrapper around Drogon
 * Provides simplified interface for routing and middleware
 */
class HttpServer {
public:
    using Handler = std::function<void(
        const HttpRequestPtr&,
        std::function<void(const HttpResponsePtr&)>&&
    )>;

    using Middleware = std::function<void(
        const HttpRequestPtr&,
        std::function<void(const HttpResponsePtr&)>&&,
        std::function<void()>&&  // next()
    )>;

    HttpServer() = default;

    /**
     * @brief Configure server settings
     */
    HttpServer& set_listen_address(const std::string& ip, uint16_t port) {
        app().addListener(ip, port);
        return *this;
    }

    HttpServer& set_threads(size_t num) {
        app().setThreadNum(num);
        return *this;
    }

    HttpServer& enable_cors() {
        app().registerPostHandlingAdvice([](const HttpRequestPtr&, const HttpResponsePtr& resp) {
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        });
        return *this;
    }

    HttpServer& set_doc_root(const std::string& path) {
        app().setDocumentRoot(path);
        return *this;
    }

    HttpServer& enable_session(size_t timeout_seconds = 1200) {
        app().enableSession(timeout_seconds);
        return *this;
    }

    /**
     * @brief Register HTTP routes
     */
    HttpServer& get(const std::string& path, Handler handler) {
        std::vector<drogon::internal::HttpConstraint> constraints;
        constraints.push_back(drogon::Get);
        app().registerHandler(
            path,
            [handler](const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
                handler(req, std::move(callback));
            },
            constraints
        );
        return *this;
    }

    HttpServer& post(const std::string& path, Handler handler) {
        std::vector<drogon::internal::HttpConstraint> constraints;
        constraints.push_back(drogon::Post);
        app().registerHandler(
            path,
            [handler](const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
                handler(req, std::move(callback));
            },
            constraints
        );
        return *this;
    }

    HttpServer& put(const std::string& path, Handler handler) {
        std::vector<drogon::internal::HttpConstraint> constraints;
        constraints.push_back(drogon::Put);
        app().registerHandler(
            path,
            [handler](const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
                handler(req, std::move(callback));
            },
            constraints
        );
        return *this;
    }

    HttpServer& del(const std::string& path, Handler handler) {
        std::vector<drogon::internal::HttpConstraint> constraints;
        constraints.push_back(drogon::Delete);
        app().registerHandler(
            path,
            [handler](const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
                handler(req, std::move(callback));
            },
            constraints
        );
        return *this;
    }

    HttpServer& route(
        const std::string& path,
        const std::vector<HttpMethod>& methods,
        Handler handler
    ) {
        return register_route(path, methods, handler);
    }

    /**
     * @brief Register global middleware
     */
    HttpServer& use(Middleware middleware) {
        middlewares_.push_back(middleware);
        return *this;
    }

    /**
     * @brief Start server (blocking)
     */
    void run() {
        // Set log level for Drogon
        app().setLogLevel(trantor::Logger::kInfo);

        // Run the event loop (blocking)
        app().run();
    }

    /**
     * @brief Start server in non-blocking mode
     */
    void run_async() {
        std::thread([this]() {
            app().run();
        }).detach();
    }

    /**
     * @brief Stop server
     */
    void stop() {
        app().quit();
    }

    /**
     * @brief Get underlying Drogon app
     */
    static drogon::HttpAppFramework& app() {
        return drogon::app();
    }

private:
    HttpServer& register_route(
        const std::string& path,
        const std::vector<HttpMethod>& methods,
        Handler handler
    ) {
        // Temporarily disable middleware chain to debug
        // Just register the handler directly
        auto simple_handler = [handler](
            const HttpRequestPtr& req,
            std::function<void(const HttpResponsePtr&)>&& callback
        ) {
            handler(req, std::move(callback));
        };

        // Convert HttpMethod to HttpConstraint
        std::vector<drogon::internal::HttpConstraint> constraints;
        for (const auto& method : methods) {
            constraints.push_back(method);
        }

        app().registerHandler(path, simple_handler, constraints);
        return *this;
    }

    void execute_middleware_chain(
        const HttpRequestPtr& req,
        std::function<void(const HttpResponsePtr&)>&& callback,
        Handler final_handler,
        size_t index
    ) {
        if (index >= middlewares_.size()) {
            // Execute final handler
            final_handler(req, std::move(callback));
            return;
        }

        // Execute current middleware
        auto& middleware = middlewares_[index];

        // Use shared_ptr to allow both callback and next to access it
        // Also use a flag to ensure callback is only called once
        struct CallbackState {
            std::function<void(const HttpResponsePtr&)> callback;
            std::atomic<bool> called{false};
        };
        auto state = std::make_shared<CallbackState>();
        state->callback = std::move(callback);

        middleware(
            req,
            [state](const HttpResponsePtr& resp) {
                // Only call callback once
                bool expected = false;
                if (state->called.compare_exchange_strong(expected, true)) {
                    state->callback(resp);
                }
            },
            [this, req, state, final_handler, index]() {
                // Only proceed if callback hasn't been called
                bool expected = false;
                if (state->called.compare_exchange_strong(expected, true)) {
                    // Move callback out of state for next chain
                    execute_middleware_chain(req, std::move(state->callback), final_handler, index + 1);
                }
            }
        );
    }

    std::vector<Middleware> middlewares_;
};

/**
 * @brief Helper functions for creating HTTP responses
 */
class Response {
public:
    static HttpResponsePtr json(
        const nlohmann::json& data,
        HttpStatusCode status = k200OK
    ) {
        // Convert nlohmann::json to Json::Value (jsoncpp)
        Json::Value json_value;
        Json::Reader reader;
        reader.parse(data.dump(), json_value);

        auto resp = HttpResponse::newHttpJsonResponse(json_value);
        resp->setStatusCode(status);
        return resp;
    }

    static HttpResponsePtr text(
        const std::string& text,
        HttpStatusCode status = k200OK
    ) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(text);
        resp->setStatusCode(status);
        resp->setContentTypeCode(CT_TEXT_PLAIN);
        return resp;
    }

    static HttpResponsePtr error(
        const std::string& message,
        HttpStatusCode status = k400BadRequest
    ) {
        nlohmann::json error_json = {
            {"error", message},
            {"status", status}
        };
        return json(error_json, status);
    }

    static HttpResponsePtr success(
        const nlohmann::json& data = nullptr
    ) {
        nlohmann::json response = {
            {"success", true}
        };
        if (!data.is_null()) {
            response["data"] = data;
        }
        return json(response);
    }
};

/**
 * @brief WebSocket server support
 */
class WebSocketServer {
public:
    using MessageHandler = std::function<void(
        const WebSocketConnectionPtr&,
        std::string&&,
        const WebSocketMessageType&
    )>;

    using ConnectionHandler = std::function<void(
        const HttpRequestPtr&,
        const WebSocketConnectionPtr&
    )>;

    using CloseHandler = std::function<void(const WebSocketConnectionPtr&)>;

    static void register_handler(
        const std::string& path,
        MessageHandler on_message,
        ConnectionHandler on_connection = nullptr,
        CloseHandler on_close = nullptr
    ) {
        // Note: Drogon WebSocket requires creating a WebSocketController class
        // This is a simplified wrapper - full implementation would need proper controller setup
        // For now, users should create WebSocketController subclasses directly
    }
};

} // namespace xpp::network
