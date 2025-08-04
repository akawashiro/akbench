#include "memcpy_mt_bandwidth.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <thread>
#include <vector>

#include "aklog.h"

#include "common.h"

BenchmarkResult MemcpyInMultiThread(uint64_t n_threads, int num_warmups,
                                    int num_iterations, uint64_t data_size) {
  std::vector<uint8_t> src = GenerateDataToSend(data_size);
  std::vector<uint8_t> dst(data_size, 0x00);
  uint64_t chunk_size = data_size / n_threads;

  auto copy_chunk = [&](uint64_t thread_id) {
    uint64_t start = thread_id * chunk_size;
    uint64_t end =
        (thread_id == n_threads - 1) ? data_size : start + chunk_size;
    std::memcpy(dst.data() + start, src.data() + start, end - start);
  };

  std::vector<double> durations;
  for (size_t i = 0; i < num_warmups + num_iterations; ++i) {
    std::fill(dst.begin(), dst.end(), 0x00);

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    for (uint64_t j = 0; j < n_threads; ++j) {
      threads.emplace_back(copy_chunk, j);
    }
    for (auto &t : threads) {
      t.join();
    }
    auto end = std::chrono::high_resolution_clock::now();

    if (num_warmups <= i) {
      const double duration =
          std::chrono::duration<double>(end - start).count();
      durations.push_back(duration);

      // Verify copied data
      if (!VerifyDataReceived(dst, data_size)) {
        AKLOG(aklog::LogLevel::ERROR,
              std::format("Data verification failed for iteration {}",
                          i - num_warmups + 1));
      } else {
        AKLOG(aklog::LogLevel::DEBUG,
              std::format("Data verification passed for iteration {}",
                          i - num_warmups + 1));
      }
    }
  }

  BenchmarkResult result =
      CalculateBandwidth(durations, num_iterations, data_size);
  AKLOG(aklog::LogLevel::INFO,
        std::format("{} threads bandwidth: {:.3f} ± {:.3f}{}.", n_threads,
                    result.average / (1 << 30), result.stddev / (1 << 30),
                    GIBYTE_PER_SEC_UNIT));

  return result;
}

BenchmarkResult RunMemcpyMtBandwidthBenchmark(int num_iterations,
                                              int num_warmups,
                                              uint64_t data_size,
                                              uint64_t num_threads) {
  AKLOG(aklog::LogLevel::DEBUG,
        std::format(
            "Starting multi-threaded memcpy bandwidth test with {} threads...",
            num_threads));
  BenchmarkResult result =
      MemcpyInMultiThread(num_threads, num_warmups, num_iterations, data_size);

  return result;
}
