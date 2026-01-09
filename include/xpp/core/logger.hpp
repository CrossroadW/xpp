#pragma once

#include <format>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace xpp::core {

/**
 * @brief Centralized logging system wrapper around spdlog
 * Supports console and file logging with rotation
 */
class Logger {
public:
  enum class Level {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
  };

  static Logger &instance() {
    static Logger logger;
    return logger;
  }

  /**
   * @brief Initialize logger with console and file output
   * @param log_dir Directory for log files
   * @param level Minimum log level
   * @param max_file_size Max size per log file (bytes)
   * @param max_files Max number of rotating files
   */
  void initialize(const std::string &log_dir = "logs",
                  Level level = Level::Info,
                  size_t max_file_size = 1024 * 1024 * 10, // 10MB
                  size_t max_files = 5) {
    try {
      // Console sink
      auto console_sink =
          std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
      console_sink->set_level(to_spdlog_level(level));

      // Rotating file sink
      auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          log_dir + "/xpp.log", max_file_size, max_files);
      file_sink->set_level(to_spdlog_level(level));

      // Create multi-sink logger
      std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
      logger_ =
          std::make_shared<spdlog::logger>("xpp", sinks.begin(), sinks.end());

      logger_->set_level(to_spdlog_level(level));
      logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
      logger_->flush_on(spdlog::level::warn);

      spdlog::set_default_logger(logger_);

    } catch (const spdlog::spdlog_ex &ex) {
      throw std::runtime_error(std::string("Logger initialization failed: ") +
                               ex.what());
    }
  }

  /**
   * @brief Set global log level
   */
  void set_level(Level level) {
    if (logger_) {
      logger_->set_level(to_spdlog_level(level));
    }
  }

  /**
   * @brief Get underlying spdlog logger
   */
  std::shared_ptr<spdlog::logger> get() { return logger_; }

private:
  // Helper template for formatting with any number of arguments
  template <typename... Args>
  std::string format_message(std::string_view fmt, Args &&...args) {
    if constexpr (sizeof...(Args) == 0) {
      return std::string(fmt);
    } else {
      return std::vformat(fmt, std::make_format_args(args...));
    }
  }

public:
  template <typename... Args> void trace(std::string_view fmt, Args &&...args) {
    if (logger_) {
      logger_->trace(format_message(fmt, args...));
    }
  }

  template <typename... Args> void debug(std::string_view fmt, Args &&...args) {
    if (logger_) {
      logger_->debug(format_message(fmt, args...));
    }
  }

  template <typename... Args> void info(std::string_view fmt, Args &&...args) {
    if (logger_) {
      logger_->info(format_message(fmt, args...));
    }
  }

  template <typename... Args> void warn(std::string_view fmt, Args &&...args) {
    if (logger_) {
      logger_->warn(format_message(fmt, args...));
    }
  }

  template <typename... Args> void error(std::string_view fmt, Args &&...args) {
    if (logger_) {
      logger_->error(format_message(fmt, args...));
    }
  }

  template <typename... Args>
  void critical(std::string_view fmt, Args &&...args) {
    if (logger_) {
      logger_->critical(format_message(fmt, args...));
    }
  }

  /**
   * @brief Flush all logs immediately
   */
  void flush() {
    if (logger_)
      logger_->flush();
  }

private:
  Logger() = default;
  ~Logger() {
    if (logger_) {
      logger_->flush();
    }
  }
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  spdlog::level::level_enum to_spdlog_level(Level level) {
    switch (level) {
    case Level::Trace:
      return spdlog::level::trace;
    case Level::Debug:
      return spdlog::level::debug;
    case Level::Info:
      return spdlog::level::info;
    case Level::Warn:
      return spdlog::level::warn;
    case Level::Error:
      return spdlog::level::err;
    case Level::Critical:
      return spdlog::level::critical;
    case Level::Off:
      return spdlog::level::off;
    default:
      return spdlog::level::info;
    }
  }

  std::shared_ptr<spdlog::logger> logger_;
};

} // namespace xpp::core

// Convenience inline functions instead of macros
namespace xpp {
  template <typename... Args>
  inline void log_trace(std::string_view fmt, Args&&... args) {
    core::Logger::instance().trace(fmt, args...);
  }

  template <typename... Args>
  inline void log_debug(std::string_view fmt, Args&&... args) {
    core::Logger::instance().debug(fmt, args...);
  }

  template <typename... Args>
  inline void log_info(std::string_view fmt, Args&&... args) {
    core::Logger::instance().info(fmt, args...);
  }

  template <typename... Args>
  inline void log_warn(std::string_view fmt, Args&&... args) {
    core::Logger::instance().warn(fmt, args...);
  }

  template <typename... Args>
  inline void log_error(std::string_view fmt, Args&&... args) {
    core::Logger::instance().error(fmt, args...);
  }

  template <typename... Args>
  inline void log_critical(std::string_view fmt, Args&&... args) {
    core::Logger::instance().critical(fmt, args...);
  }
} // namespace xpp
