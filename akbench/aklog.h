#pragma once

#include <string>

namespace aklog {

enum class LogLevel { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, FATAL = 4 };

void log(LogLevel level, const std::string &message);

void setLogLevel(LogLevel level);

void check(bool condition, const std::string &message);

LogLevel getLogLevel();

const char *logLevelToString(LogLevel level);

LogLevel stringToLogLevel(const std::string &level_str);

} // namespace aklog