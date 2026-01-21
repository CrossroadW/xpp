#include "xpp/core/logger.hpp"
#include <gtest/gtest.h>
#include <sstream>


class LoggerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize logger before each test
    xpp::Logger::instance().initialize();
  }
};

TEST_F(LoggerTest, LoggerInitializes) {
  EXPECT_NO_THROW(xpp::Logger::instance().initialize());
}

TEST_F(LoggerTest, LogInfoMessage) {
  EXPECT_NO_THROW(xpp::log_info("Test info message: {}", 42));
}

TEST_F(LoggerTest, LogWarningMessage) {
  EXPECT_NO_THROW(xpp::log_warn("Test warning message: {}", "warning"));
}

TEST_F(LoggerTest, LogErrorMessage) {
  EXPECT_NO_THROW(xpp::log_error("Test error message: {}", 3.14));
}

TEST_F(LoggerTest, LogDebugMessage) {
  EXPECT_NO_THROW(xpp::log_debug("Test debug message: {}", true));
}

TEST_F(LoggerTest, MultipleLogsWithDifferentTypes) {
  EXPECT_NO_THROW({
    xpp::log_info("String: {}", "test");
    xpp::log_info("Integer: {}", 123);
    xpp::log_info("Float: {}", 45.67);
    xpp::log_info("Bool: {}", false);
  });
}
