#pragma once

#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <fmt/format.h>
#include <memory>
#include <string>
#include <optional>
#include <stdexcept>
#include <xpp/core/logger.hpp>
namespace xpp::infrastructure {

using namespace drogon::orm;

/**
 * @brief Database connection pool manager
 * Wraps Drogon's ORM for simplified database access
 */
class DatabasePool {
public:
    struct Config {
        std::string host = "localhost";
        uint16_t port = 5432;
        std::string database = "xpp";
        std::string username = "postgres";
        std::string password = "";
        size_t connection_num = 10;
        bool auto_reconnect = true;
        double timeout = 10.0;  // seconds
    };

    static DatabasePool& instance() {
        static DatabasePool pool;
        return pool;
    }

    /**
     * @brief Initialize connection pool
     */
    void initialize(const Config& config) {
        std::string connection_string = fmt::format(
            "host={} port={} dbname={} user={} password={}",
            config.host, config.port, config.database,
            config.username, config.password
        );

        client_ = drogon::orm::DbClient::newPgClient(
            connection_string,
            config.connection_num
        );

        if (!client_) {
            throw std::runtime_error("Failed to create database client");
        }

        xpp::log_info("Database pool initialized: {}:{}/{}",
                 config.host, config.port, config.database);
    }

    /**
     * @brief Get database client
     */
    DbClientPtr client() {
        if (!client_) {
            throw std::runtime_error("Database not initialized");
        }
        return client_;
    }

    /**
     * @brief Execute raw SQL query
     */
    template<typename Callback>
    void execute(const std::string& sql, Callback&& callback) {
        client_->execSqlAsync(
            sql,
            [cb = std::forward<Callback>(callback)](const Result& result) mutable {
                cb(result, std::nullopt);
            },
            [cb = std::forward<Callback>(callback)](const DrogonDbException& e) mutable {
                cb(Result{}, std::optional<std::string>(e.base().what()));
            }
        );
    }

    /**
     * @brief Execute SQL with parameters (prepared statement)
     */
    template<typename Callback, typename... Args>
    void execute(const std::string& sql, Callback&& callback, Args&&... args) {
        client_->execSqlAsync(
            sql,
            [cb = std::forward<Callback>(callback)](const Result& result) mutable {
                cb(result, std::nullopt);
            },
            [cb = std::forward<Callback>(callback)](const DrogonDbException& e) mutable {
                cb(Result{}, std::optional<std::string>(e.base().what()));
            },
            std::forward<Args>(args)...
        );
    }

    /**
     * @brief Execute query synchronously (blocking)
     */
    template<typename... Args>
    Result execute_sync(const std::string& sql, Args&&... args) {
        try {
            return client_->execSqlSync(sql, std::forward<Args>(args)...);
        } catch (const DrogonDbException& e) {
            throw;
        }
    }

    /**
     * @brief Begin transaction
     */
    template<typename Callback>
    void transaction(Callback&& callback) {
        auto trans = client_->newTransaction();

        try {
            callback(trans);
        } catch (const std::exception& e) {
            trans->rollback();
            throw;
        }
    }

    /**
     * @brief Check if database is connected
     */
    bool is_connected() const {
        return client_ != nullptr;
    }

    /**
     * @brief Close all connections
     */
    void close() {
        if (client_) {
            client_->closeAll();
            client_.reset();
        }
    }

private:
    DatabasePool() = default;
    ~DatabasePool() {
        close();
    }
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;

    DbClientPtr client_;
};

/**
 * @brief RAII transaction guard
 */
class Transaction {
public:
    explicit Transaction(DbClientPtr client)
        : trans_(client->newTransaction()), committed_(false) {}

    ~Transaction() {
        if (!committed_) {
            trans_->rollback();
        }
    }

    auto operator->() { return trans_.operator->(); }

    void commit() {
        committed_ = true;
    }

    void rollback() {
        trans_->rollback();
        committed_ = true;
    }

private:
    std::shared_ptr<drogon::orm::Transaction> trans_;
    bool committed_;
};

} // namespace xpp::infrastructure
