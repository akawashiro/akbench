#include "aklog.h"

#include <atomic>
#include <cstdlib>
#include <format>
#include <iostream>
#include <print>

namespace aklog {

namespace {
std::atomic<LogLevel> g_log_level{LogLevel::INFO};
}

void log(LogLevel level, const std::string& message) {
  if (level < g_log_level.load()) {
    return;
  }

  std::print(std::cerr, "[{}] {}\n", logLevelToString(level), message);

  if (level == LogLevel::FATAL) {
    std::abort();
  }
}

void setLogLevel(LogLevel level) {
  g_log_level.store(level);
}

void check(bool condition, const std::string& message) {
  if (!condition) {
    std::print(std::cerr, "[FATAL] Check failed: {}\n", message);
    std::abort();
  }
}

LogLevel getLogLevel() {
  return g_log_level.load();
}

const char* logLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::FATAL:
      return "FATAL";
  }
  return "UNKNOWN";
}

LogLevel stringToLogLevel(const std::string& level_str) {
  if (level_str == "DEBUG") return LogLevel::DEBUG;
  if (level_str == "INFO") return LogLevel::INFO;
  if (level_str == "WARNING") return LogLevel::WARNING;
  if (level_str == "ERROR") return LogLevel::ERROR;
  if (level_str == "FATAL") return LogLevel::FATAL;
  return LogLevel::INFO;  // Default fallback
}

}  // namespace aklog