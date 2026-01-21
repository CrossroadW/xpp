#pragma once

#include <sqlite3.h>
#include <fmt/format.h>
#include <memory>
#include <string>
#include <optional>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <xpp/core/logger.hpp>

namespace xpp::infrastructure {
// Forward declare Transaction for begin_transaction API
class Transaction;
/**
 * @brief Database connection pool manager
 * Wraps Drogon's ORM for simplified database access
/**
 * @brief Simple wrapper for SQLite3 result
 */
struct QueryResult {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> columns;
    bool is_success = true;
    std::string error_message;

    bool empty() const { return rows.empty(); }
    size_t size() const { return rows.size(); }

    const std::vector<std::string>& operator[](size_t index) const {
        if (index >= rows.size()) throw std::out_of_range("Row index out of range");
        return rows[index];
    }
};

class DatabasePool {
public:
    struct Config {
        std::string database_file = "xpp.db"; // SQLite file path
        std::string database = "test";
        bool auto_create = true;
    };

    static DatabasePool& instance() {
        static DatabasePool pool;
        return pool;
    }

    void initialize(const Config& config) {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        
        int rc = sqlite3_open(config.database.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = db_ ? sqlite3_errmsg(db_) : "unknown";
            throw std::runtime_error(fmt::format("Failed to open database: {}", msg));
        }

        // Enable foreign keys and set busy timeout
        execute_sync("PRAGMA foreign_keys = ON");
        sqlite3_busy_timeout(db_, 5000);

        xpp::log_info("SQLite3 database initialized: {}", config.database);
    }

    QueryResult execute_sync(const std::string& sql) {
        if (!db_) throw std::runtime_error("Database not initialized");

        QueryResult result;
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            result.is_success = false;
            result.error_message = sqlite3_errmsg(db_);
            return result;
        }

        int column_count = sqlite3_column_count(stmt);
        for (int i = 0; i < column_count; ++i) {
            result.columns.push_back(sqlite3_column_name(stmt, i));
        }

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            std::vector<std::string> row;
            for (int i = 0; i < column_count; ++i) {
                const unsigned char* text = sqlite3_column_text(stmt, i);
                row.push_back(text ? reinterpret_cast<const char*>(text) : std::string());
            }
            result.rows.push_back(std::move(row));
        }

        if (rc != SQLITE_DONE) {
            result.is_success = false;
            result.error_message = sqlite3_errmsg(db_);
        }

        sqlite3_finalize(stmt);
        return result;
    }

    template<typename... Args>
    QueryResult execute_sync(const std::string& sql, Args&&... args) {
        std::string final_sql = fmt::vformat(sql, fmt::make_format_args(args...));
        return execute_sync(final_sql);
    }

    bool is_connected() const { return db_ != nullptr; }

    void close() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    sqlite3* get_handle() { return db_; }

    // Transaction helpers (internal)
    void exec_begin_transaction() { execute_sync("BEGIN TRANSACTION"); }
    void exec_commit_transaction() { execute_sync("COMMIT"); }
    void exec_rollback_transaction() { execute_sync("ROLLBACK"); }

    // Begin a Transaction object (returns RAII transaction)
    Transaction begin_transaction();

    int64_t last_insert_id() {
        return db_ ? sqlite3_last_insert_rowid(db_) : 0;
    }

private:
    DatabasePool() = default;
    ~DatabasePool() { close(); }
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;

    sqlite3* db_ = nullptr;
};

class Transaction {
public:
    explicit Transaction(DatabasePool& pool) : pool_(pool), committed_(false) { pool_.exec_begin_transaction(); }
    ~Transaction() { if (!committed_) pool_.exec_rollback_transaction(); }
    void commit() { pool_.exec_commit_transaction(); committed_ = true; }
    void rollback() { pool_.exec_rollback_transaction(); committed_ = true; }
private:
    DatabasePool& pool_;
    bool committed_;
};

// Return an RAII Transaction object
inline Transaction DatabasePool::begin_transaction() {
    return Transaction(*this);
}

} // namespace xpp::infrastructure
