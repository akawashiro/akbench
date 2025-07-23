#include "memcpy_benchmark.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

#include "common.h"

int RunMemcpyBenchmark(int num_iterations, int num_warmups,
                       uint64_t data_size) {
  std::vector<uint8_t> src = GenerateDataToSend(data_size);
  std::vector<uint8_t> dst(data_size, 0);
  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    std::fill(dst.begin(), dst.end(), 0);
    const auto start = std::chrono::high_resolution_clock::now();
    std::memcpy(dst.data(), src.data(), data_size);
    const auto end = std::chrono::high_resolution_clock::now();

    CHECK(VerifyDataReceived(src, data_size))
        << "Data verification failed before memcpy.";
    if (num_warmups <= iteration) {
      const double duration =
          std::chrono::duration<double>(end - start).count();
      durations.push_back(duration);
    }
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  LOG(INFO) << "Bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec";

  return 0;
}
