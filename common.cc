#include "common.h"

#include <algorithm>
#include <numeric>
#include <random>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"

std::vector<uint8_t> CalcChecksum(const std::vector<uint8_t> &data,
                                  uint64_t data_size) {
  CHECK(data_size > CHECKSUM_SIZE)
      << "data_size (" << data_size << ") must be greater than CHECKSUM_SIZE ("
      << CHECKSUM_SIZE << ")";
  uint64_t context_size = data_size - CHECKSUM_SIZE;
  std::vector<uint8_t> checksum(CHECKSUM_SIZE, 0);
  for (size_t i = 0; i < context_size; ++i) {
    checksum[i % CHECKSUM_SIZE] ^= data[i];
  }
  return checksum;
}

std::vector<uint8_t> GenerateDataToSend(uint64_t data_size) {
  CHECK(data_size > CHECKSUM_SIZE)
      << "data_size (" << data_size << ") must be greater than CHECKSUM_SIZE ("
      << CHECKSUM_SIZE << ")";
  uint64_t context_size = data_size - CHECKSUM_SIZE;
  VLOG(1) << "Generating data to send...";
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
  VLOG(1) << "Context data generated. Size: " << context_size
          << " bytes. Filling checksum...";
  const std::vector<uint8_t> checksum = CalcChecksum(data, data_size);
  for (size_t j = 0; j < CHECKSUM_SIZE; ++j) {
    data[context_size + j] = checksum[j];
  }
  VLOG(1) << "Data generation complete. Data size: "
          << static_cast<double>(data.size()) / (1 << 30)
          << " GiByte, Checksum size: " << CHECKSUM_SIZE << " bytes.";

  return data;
}

bool VerifyDataReceived(const std::vector<uint8_t> &data, uint64_t data_size) {
  CHECK(data_size > CHECKSUM_SIZE)
      << "data_size (" << data_size << ") must be greater than CHECKSUM_SIZE ("
      << CHECKSUM_SIZE << ")";
  uint64_t context_size = data_size - CHECKSUM_SIZE;
  if (data.size() != data_size) {
    LOG(ERROR) << "Data size mismatch: expected " << data_size << ", got "
               << data.size();
    return false;
  }

  std::vector<uint8_t> checksum = CalcChecksum(data, data_size);
  for (size_t i = 0; i < CHECKSUM_SIZE; ++i) {
    if (data[context_size + i] != checksum[i]) {
      LOG(ERROR) << "Checksum mismatch at index " << i << ": expected "
                 << static_cast<int>(checksum[i]) << ", got "
                 << static_cast<int>(data[context_size + i]);
      return false;
    }
  }
  return true;
}

double CalculateBandwidth(std::vector<double> durations, int num_iterations,
                          uint64_t data_size) {
  CHECK(durations.size() == num_iterations);
  std::sort(durations.begin(), durations.end());
  // Ensure we have at least 3 iterations to remove min and max
  CHECK(num_iterations >= 3) << "num_iterations must be at least 3";
  std::vector<double> filtered_durations(durations.begin() + 1,
                                         durations.end() - 1);

  double average_duration = std::accumulate(filtered_durations.begin(),
                                            filtered_durations.end(), 0.0) /
                            filtered_durations.size();
  double bandwidth = data_size / average_duration;
  return bandwidth;
}

std::string ReceivePrefix(int iteration) {
  int pid = getpid();
  return absl::StrCat("Receive (PID ", pid, ", iteration ", iteration, "): ");
}

std::string SendPrefix(int iteration) {
  int pid = getpid();
  return absl::StrCat("Send (PID ", pid, ", iteration ", iteration, "): ");
}
