#include "aklog.h"

#include <cassert>
#include <print>
#include <string>

namespace {

void testLogLevelToString() {
  assert(std::string(aklog::logLevelToString(aklog::LogLevel::DEBUG)) ==
         "DEBUG");
  assert(std::string(aklog::logLevelToString(aklog::LogLevel::INFO)) == "INFO");
  assert(std::string(aklog::logLevelToString(aklog::LogLevel::WARNING)) ==
         "WARNING");
  assert(std::string(aklog::logLevelToString(aklog::LogLevel::ERROR)) ==
         "ERROR");
  assert(std::string(aklog::logLevelToString(aklog::LogLevel::FATAL)) ==
         "FATAL");
  std::print("testLogLevelToString passed\n");
}

void testStringToLogLevel() {
  assert(aklog::stringToLogLevel("DEBUG") == aklog::LogLevel::DEBUG);
  assert(aklog::stringToLogLevel("INFO") == aklog::LogLevel::INFO);
  assert(aklog::stringToLogLevel("WARNING") == aklog::LogLevel::WARNING);
  assert(aklog::stringToLogLevel("ERROR") == aklog::LogLevel::ERROR);
  assert(aklog::stringToLogLevel("FATAL") == aklog::LogLevel::FATAL);
  assert(aklog::stringToLogLevel("INVALID") ==
         aklog::LogLevel::INFO); // Default fallback
  std::print("testStringToLogLevel passed\n");
}

void testSetAndGetLogLevel() {
  // Test default level is INFO
  assert(aklog::getLogLevel() == aklog::LogLevel::INFO);

  // Test setting different levels
  aklog::setLogLevel(aklog::LogLevel::DEBUG);
  assert(aklog::getLogLevel() == aklog::LogLevel::DEBUG);

  aklog::setLogLevel(aklog::LogLevel::WARNING);
  assert(aklog::getLogLevel() == aklog::LogLevel::WARNING);

  aklog::setLogLevel(aklog::LogLevel::ERROR);
  assert(aklog::getLogLevel() == aklog::LogLevel::ERROR);

  // Reset to INFO for other tests
  aklog::setLogLevel(aklog::LogLevel::INFO);
  std::print("testSetAndGetLogLevel passed\n");
}

void testLogFiltering() {
  // Set log level to WARNING to filter out DEBUG and INFO
  aklog::setLogLevel(aklog::LogLevel::WARNING);

  std::print("Testing log filtering (only WARNING and ERROR should appear):\n");
  AKLOG(aklog::LogLevel::DEBUG, "This DEBUG message should not appear");
  AKLOG(aklog::LogLevel::INFO, "This INFO message should not appear");
  AKLOG(aklog::LogLevel::WARNING, "This WARNING message should appear");
  AKLOG(aklog::LogLevel::ERROR, "This ERROR message should appear");

  // Reset to INFO
  aklog::setLogLevel(aklog::LogLevel::INFO);
  std::print("testLogFiltering passed\n");
}

void testCheckSuccess() {
  // Test check with true condition (should not abort)
  AKCHECK(true, "This check should pass");
  AKCHECK(1 == 1, "Math still works");
  AKCHECK(!false, "Logic still works");
  std::print("testCheckSuccess passed\n");
}

void testLogMessages() {
  std::print("Testing log messages at different levels:\n");
  aklog::setLogLevel(aklog::LogLevel::DEBUG);

  AKLOG(aklog::LogLevel::DEBUG, "Debug message for testing");
  AKLOG(aklog::LogLevel::INFO, "Info message for testing");
  AKLOG(aklog::LogLevel::WARNING, "Warning message for testing");
  AKLOG(aklog::LogLevel::ERROR, "Error message for testing");

  // Reset to INFO
  aklog::setLogLevel(aklog::LogLevel::INFO);
  std::print("testLogMessages passed\n");
}

} // namespace

int main() {
  std::print("Running aklog tests...\n");

  testLogLevelToString();
  testStringToLogLevel();
  testSetAndGetLogLevel();
  testLogFiltering();
  testCheckSuccess();
  testLogMessages();

  std::print("All aklog tests passed!\n");

  // Note: We cannot test FATAL logging or check failure because they abort the
  // process
  std::print("Note: FATAL logging and check failure tests are not run as they "
             "would abort the process.\n");

  return 0;
}