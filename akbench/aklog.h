#pragma once

#include <string>

namespace aklog {

enum class LogLevel { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, FATAL = 4 };

void log(LogLevel level, const std::string &message, const char *file,
         int line);

void setLogLevel(LogLevel level);

void check(bool condition, const std::string &message, const char *file,
           int line);

LogLevel getLogLevel();

const char *logLevelToString(LogLevel level);

LogLevel stringToLogLevel(const std::string &level_str);

// Macros for convenient logging with file and line information
#define AKLOG(level, message) aklog::log(level, message, __FILE__, __LINE__)
#define AKCHECK(condition, message)                                            \
  aklog::check(condition, message, __FILE__, __LINE__)

} // namespace aklog