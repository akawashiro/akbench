#include "common.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>
#include <random>
#include <unistd.h>

#include "aklog.h"

std::vector<uint8_t> CalcChecksum(const std::vector<uint8_t> &data,
                                  uint64_t data_size) {
  AKCHECK(data_size > CHECKSUM_SIZE,
          std::format("data_size ({}) must be greater than CHECKSUM_SIZE ({})",
                      data_size, CHECKSUM_SIZE));
  uint64_t context_size = data_size - CHECKSUM_SIZE;
  std::vector<uint8_t> checksum(CHECKSUM_SIZE, 0);
  for (size_t i = 0; i < context_size; ++i) {
    checksum[i % CHECKSUM_SIZE] ^= data[i];
  }
  return checksum;
}

std::vector<uint8_t> GenerateDataToSend(uint64_t data_size) {
  AKCHECK(data_size > CHECKSUM_SIZE,
          std::format("data_size ({}) must be greater than CHECKSUM_SIZE ({})",
                      data_size, CHECKSUM_SIZE));
  uint64_t context_size = data_size - CHECKSUM_SIZE;
  AKLOG(aklog::LogLevel::DEBUG, "Generating data to send...");
  std::random_device seed_gen;
  std::mt19937 engine(seed_gen());
  std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
  std::vector<uint8_t> data(data_size, 0);

  size_t i = 0;
  for (i = 0; i + 8 < context_size; i += 8) {
    uint64_t *d = reinterpret_cast<uint64_t *>(&data[i]);
    *d = dist(engine);
  }
  for (; i < context_size; ++i) {
    data[i] = static_cast<uint8_t>(dist(engine) & 0xFF);
  }
  AKLOG(
      aklog::LogLevel::DEBUG,
      std::format("Context data generated. Size: {} bytes. Filling checksum...",
                  context_size));
  const std::vector<uint8_t> checksum = CalcChecksum(data, data_size);
  for (size_t j = 0; j < CHECKSUM_SIZE; ++j) {
    data[context_size + j] = checksum[j];
  }
  AKLOG(aklog::LogLevel::DEBUG,
        std::format("Data generation complete. Data size: {} GiByte, Checksum "
                    "size: {} bytes.",
                    static_cast<double>(data.size()) / (1 << 30),
                    CHECKSUM_SIZE));

  return data;
}

bool VerifyDataReceived(const std::vector<uint8_t> &data, uint64_t data_size) {
  AKCHECK(data_size > CHECKSUM_SIZE,
          std::format("data_size ({}) must be greater than CHECKSUM_SIZE ({})",
                      data_size, CHECKSUM_SIZE));
  uint64_t context_size = data_size - CHECKSUM_SIZE;
  if (data.size() != data_size) {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("Data size mismatch: expected {}, got {}", data_size,
                      data.size()));
    return false;
  }

  std::vector<uint8_t> checksum = CalcChecksum(data, data_size);
  for (size_t i = 0; i < CHECKSUM_SIZE; ++i) {
    if (data[context_size + i] != checksum[i]) {
      AKLOG(aklog::LogLevel::ERROR,
            std::format("Checksum mismatch at index {}: expected {}, got {}", i,
                        static_cast<int>(checksum[i]),
                        static_cast<int>(data[context_size + i])));
      return false;
    }
  }
  return true;
}

BenchmarkResult CalculateBandwidth(std::vector<double> durations,
                                   int num_iterations, uint64_t data_size) {
  AKCHECK(durations.size() == num_iterations,
          std::format("durations.size() ({}) must equal num_iterations ({})",
                      durations.size(), num_iterations));
  std::sort(durations.begin(), durations.end());
  // Ensure we have at least 3 iterations to remove min and max
  AKCHECK(num_iterations >= 3, "num_iterations must be at least 3");
  std::vector<double> filtered_durations(durations.begin() + 1,
                                         durations.end() - 1);

  double average_duration = std::accumulate(filtered_durations.begin(),
                                            filtered_durations.end(), 0.0) /
                            filtered_durations.size();

  // Calculate standard deviation
  double variance = 0.0;
  for (const auto &duration : filtered_durations) {
    variance += std::pow(duration - average_duration, 2);
  }
  variance /= filtered_durations.size();
  double stddev_duration = std::sqrt(variance);

  double bandwidth = data_size / average_duration;
  double bandwidth_stddev =
      data_size * stddev_duration / (average_duration * average_duration);

  return BenchmarkResult{bandwidth, bandwidth_stddev};
}

BenchmarkResult CalculateOneTripDuration(const std::vector<double> &durations) {
  AKCHECK(durations.size() >= 3,
          std::format("durations.size() ({}) must be at least 3",
                      durations.size()));
  std::vector<double> sorted_durations = durations;
  std::sort(sorted_durations.begin(), sorted_durations.end());

  // Calculate average of filtered durations
  double average_duration = std::accumulate(sorted_durations.begin() + 1,
                                            sorted_durations.end() - 1, 0.0) /
                            (sorted_durations.size() - 2);

  // Calculate standard deviation of filtered durations
  double variance = 0.0;
  for (size_t i = 1; i < sorted_durations.size() - 1; ++i) {
    variance += std::pow(sorted_durations[i] - average_duration, 2);
  }
  variance /= (sorted_durations.size() - 2);
  double stddev_duration = std::sqrt(variance);

  return BenchmarkResult{average_duration, stddev_duration};
}

std::string ReceivePrefix(int iteration) {
  int pid = getpid();
  return std::format("Receive (PID {}, iteration {}): ", pid, iteration);
}

std::string SendPrefix(int iteration) {
  int pid = getpid();
  return std::format("Send (PID {}, iteration {}): ", pid, iteration);
}

std::string GenerateUniqueName(const std::string &base_name) {
  std::random_device seed_gen;
  std::mt19937 engine(seed_gen());
  std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);

  uint32_t random_value = dist(engine);
  std::string hex_suffix = std::format("{:08x}", random_value);

  // Find the last dot for file extension
  size_t dot_pos = base_name.rfind('.');
  if (dot_pos != std::string::npos) {
    // Insert suffix before extension
    return base_name.substr(0, dot_pos) + "_" + hex_suffix +
           base_name.substr(dot_pos);
  } else {
    // No extension, just append suffix
    return base_name + "_" + hex_suffix;
  }
}
