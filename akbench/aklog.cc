#include "aklog.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <print>
#include <thread>
#include <unistd.h>

namespace aklog {

namespace {
std::atomic<LogLevel> g_log_level{LogLevel::INFO};

char logLevelToChar(LogLevel level) {
  switch (level) {
  case LogLevel::DEBUG:
    return 'D';
  case LogLevel::INFO:
    return 'I';
  case LogLevel::WARNING:
    return 'W';
  case LogLevel::ERROR:
    return 'E';
  case LogLevel::FATAL:
    return 'F';
  }
  return 'U';
}

std::string getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()) %
            1000000;

  struct tm *tm_info = localtime(&time_t);
  return std::format("{:02d}{:02d} {:02d}:{:02d}:{:02d}.{:06d}",
                     tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour,
                     tm_info->tm_min, tm_info->tm_sec, ms.count());
}
} // namespace

void log(LogLevel level, const std::string &message, const char *file,
         int line) {
  if (level < g_log_level.load()) {
    return;
  }

  std::string timestamp = getCurrentTimestamp();
  pid_t pid = getpid();
  std::thread::id thread_id = std::this_thread::get_id();

  // Extract just the filename from the full path
  const char *filename = file;
  const char *last_slash = strrchr(file, '/');
  if (last_slash) {
    filename = last_slash + 1;
  }

  std::print(std::cerr, "{}{} {}  {} {}:{}] {}\n", logLevelToChar(level),
             timestamp, pid, reinterpret_cast<uintptr_t>(&thread_id), filename,
             line, message);

  if (level == LogLevel::FATAL) {
    std::abort();
  }
}

void setLogLevel(LogLevel level) { g_log_level.store(level); }

void check(bool condition, const std::string &message, const char *file,
           int line) {
  if (!condition) {
    log(LogLevel::FATAL, std::format("Check failed: {}", message), file, line);
  }
}

LogLevel getLogLevel() { return g_log_level.load(); }

const char *logLevelToString(LogLevel level) {
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

LogLevel stringToLogLevel(const std::string &level_str) {
  if (level_str == "DEBUG")
    return LogLevel::DEBUG;
  if (level_str == "INFO")
    return LogLevel::INFO;
  if (level_str == "WARNING")
    return LogLevel::WARNING;
  if (level_str == "ERROR")
    return LogLevel::ERROR;
  if (level_str == "FATAL")
    return LogLevel::FATAL;
  return LogLevel::INFO; // Default fallback
}

} // namespace aklog