#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr uint64_t CHECKSUM_SIZE = 128;
constexpr const char *GIBYTE_PER_SEC_UNIT = " GiByte/sec";

struct BenchmarkResult {
  double average;
  double stddev;
};

std::vector<uint8_t> GenerateDataToSend(uint64_t data_size);
bool VerifyDataReceived(const std::vector<uint8_t> &data, uint64_t data_size);
BenchmarkResult CalculateBandwidth(const std::vector<double> &durations,
                                   int num_iterations, uint64_t data_size);
BenchmarkResult CalculateOneTripDuration(const std::vector<double> &durations);
std::string ReceivePrefix(int iteration);
std::string SendPrefix(int iteration);
std::string GenerateUniqueName(const std::string &base_name);
