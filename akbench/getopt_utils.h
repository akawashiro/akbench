#pragma once

#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>

// Helper functions for parsing command line arguments with getopt_long

// Parse a string to uint64_t
inline std::optional<uint64_t> ParseUint64(const std::string &str) {
  if (str.empty()) {
    return std::nullopt;
  }

  uint64_t value;
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

  if (ec == std::errc() && ptr == str.data() + str.size()) {
    return value;
  }

  // Try to parse expressions like "1 << 30" for 1GB
  if (str.find("<<") != std::string::npos) {
    size_t pos = str.find("<<");
    std::string base_str = str.substr(0, pos);
    std::string shift_str = str.substr(pos + 2);

    // Remove spaces
    base_str.erase(std::remove(base_str.begin(), base_str.end(), ' '),
                   base_str.end());
    shift_str.erase(std::remove(shift_str.begin(), shift_str.end(), ' '),
                    shift_str.end());

    uint64_t base, shift;
    auto [base_ptr, base_ec] = std::from_chars(
        base_str.data(), base_str.data() + base_str.size(), base);
    auto [shift_ptr, shift_ec] = std::from_chars(
        shift_str.data(), shift_str.data() + shift_str.size(), shift);

    if (base_ec == std::errc() && shift_ec == std::errc() &&
        base_ptr == base_str.data() + base_str.size() &&
        shift_ptr == shift_str.data() + shift_str.size()) {
      return base << shift;
    }
  }

  throw std::invalid_argument(std::format("Invalid uint64_t value: '{}'", str));
}

// Parse a string to int
inline int ParseInt(const std::string &str) {
  if (str.empty()) {
    throw std::invalid_argument("Empty string cannot be parsed as int");
  }

  int value;
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

  if (ec == std::errc() && ptr == str.data() + str.size()) {
    return value;
  }

  throw std::invalid_argument(std::format("Invalid int value: '{}'", str));
}

// Helper to print an error message and exit
inline void PrintErrorAndExit(const std::string &program_name,
                              const std::string &error_msg) {
  std::fprintf(stderr, "%s: %s\n", program_name.c_str(), error_msg.c_str());
  std::fprintf(stderr, "Try '%s --help' for more information.\n",
               program_name.c_str());
  std::exit(1);
}