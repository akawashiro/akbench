#include <iostream>
#include <optional>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"

#include "common.h"
#include "fifo_benchmark.h"
#include "memcpy_benchmark.h"
#include "memcpy_mt_benchmark.h"
#include "mmap_benchmark.h"
#include "pipe_benchmark.h"
#include "shm_benchmark.h"
#include "tcp_benchmark.h"
#include "uds_benchmark.h"

ABSL_FLAG(std::string, type, "",
          "Benchmark type to run (memcpy, memcpy_mt, tcp, uds, pipe, fifo, "
          "mmap, shm)");
ABSL_FLAG(int, num_iterations, 10,
          "Number of measurement iterations (minimum 3)");
ABSL_FLAG(int, num_warmups, 3, "Number of warmup iterations");
ABSL_FLAG(uint64_t, data_size, (1 << 30), "Size of data to transfer in bytes");
ABSL_FLAG(std::optional<uint64_t>, buffer_size, std::nullopt,
          "Buffer size for I/O operations in bytes (default: 1 MiByte, not "
          "applicable to memcpy benchmarks)");
ABSL_FLAG(std::optional<uint64_t>, num_threads, std::nullopt,
          "Number of threads for memcpy_mt benchmark (default: run with 1-4 "
          "threads)");
ABSL_FLAG(std::optional<int>, vlog, std::nullopt,
          "Show VLOG messages lower than this level.");

constexpr uint64_t DEFAULT_BUFFER_SIZE = 1 << 20; // 1 MiByte

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage(
      "Bandwidth benchmark tool. Use --type to specify benchmark type.");
  absl::ParseCommandLine(argc, argv);

  // Get values from command line flags
  std::string type = absl::GetFlag(FLAGS_type);
  int num_iterations = absl::GetFlag(FLAGS_num_iterations);
  int num_warmups = absl::GetFlag(FLAGS_num_warmups);
  uint64_t data_size = absl::GetFlag(FLAGS_data_size);
  std::optional<uint64_t> buffer_size_opt = absl::GetFlag(FLAGS_buffer_size);
  std::optional<uint64_t> num_threads_opt = absl::GetFlag(FLAGS_num_threads);

  // Validate type
  if (type.empty()) {
    LOG(ERROR) << "Must specify --type. Available types: memcpy, memcpy_mt, "
                  "tcp, uds, pipe, fifo, mmap, shm, all";
    return 1;
  }

  // Check if buffer_size is specified for incompatible benchmark types
  if ((type == "memcpy" || type == "memcpy_mt") &&
      buffer_size_opt.has_value()) {
    LOG(ERROR) << "Buffer size option is not applicable to " << type
               << " benchmark type";
    return 1;
  }

  // Check if num_threads is specified for incompatible benchmark types
  if (type != "memcpy_mt" && num_threads_opt.has_value()) {
    LOG(ERROR) << "Number of threads option is only applicable to memcpy_mt "
                  "benchmark type";
    return 1;
  }

  // Validate num_threads for memcpy_mt
  if (type == "memcpy_mt" && num_threads_opt.has_value() &&
      num_threads_opt.value() == 0) {
    LOG(ERROR) << "num_threads must be greater than 0, got: "
               << num_threads_opt.value();
    return 1;
  }

  // Get buffer size (use default if not specified)
  uint64_t buffer_size = buffer_size_opt.value_or(DEFAULT_BUFFER_SIZE);

  // Validate buffer_size
  if (buffer_size == 0) {
    LOG(ERROR) << "buffer_size must be greater than 0, got: " << buffer_size;
    return 1;
  }

  if (buffer_size > data_size) {
    LOG(ERROR) << "buffer_size (" << buffer_size
               << ") cannot be larger than data_size (" << data_size << ")";
    return 1;
  }

  // Validate num_iterations
  if (num_iterations < 3) {
    LOG(ERROR) << "num_iterations must be at least 3, got: " << num_iterations;
    return 1;
  }

  // Validate data_size
  if (data_size <= CHECKSUM_SIZE) {
    LOG(ERROR) << "data_size must be larger than CHECKSUM_SIZE ("
               << CHECKSUM_SIZE << "), got: " << data_size;
    return 1;
  }

  std::optional<int> vlog = absl::GetFlag(FLAGS_vlog);
  if (vlog.has_value()) {
    int v = *vlog;
    absl::SetGlobalVLogLevel(v);
  }

  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  absl::InitializeLog();

  // Run the appropriate benchmark
  double bandwidth = 0.0;

  if (type == "all") {
    // Collect all benchmark results first, then output at the end
    std::vector<std::pair<std::string, double>> results;

    bandwidth = RunMemcpyBenchmark(num_iterations, num_warmups, data_size);
    results.emplace_back("memcpy", bandwidth);

    // Run memcpy_mt with 1-4 threads for "all" case
    for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
      bandwidth = RunMemcpyMtBenchmark(num_iterations, num_warmups, data_size,
                                       n_threads);
      results.emplace_back(
          "memcpy_mt (" + std::to_string(n_threads) + " threads)", bandwidth);
    }

    bandwidth =
        RunTcpBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    results.emplace_back("tcp", bandwidth);

    bandwidth =
        RunUdsBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    results.emplace_back("uds", bandwidth);

    bandwidth =
        RunPipeBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    results.emplace_back("pipe", bandwidth);

    bandwidth =
        RunFifoBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    results.emplace_back("fifo", bandwidth);

    bandwidth =
        RunMmapBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    results.emplace_back("mmap", bandwidth);

    bandwidth =
        RunShmBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    results.emplace_back("shm", bandwidth);

    // Output all results at the end
    for (const auto &result : results) {
      std::cout << result.first << ": " << result.second / (1ULL << 30)
                << GIBYTE_PER_SEC_UNIT << std::endl;
    }

    return 0;
  } else if (type == "memcpy") {
    bandwidth = RunMemcpyBenchmark(num_iterations, num_warmups, data_size);
  } else if (type == "memcpy_mt") {
    if (num_threads_opt.has_value()) {
      // Run with specified number of threads
      bandwidth = RunMemcpyMtBenchmark(num_iterations, num_warmups, data_size,
                                       num_threads_opt.value());
    } else {
      // Run with 1-4 threads for compatibility
      for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
        bandwidth = RunMemcpyMtBenchmark(num_iterations, num_warmups, data_size,
                                         n_threads);
        std::cout << "memcpy_mt (" << n_threads
                  << " threads): " << bandwidth / (1ULL << 30)
                  << GIBYTE_PER_SEC_UNIT << std::endl;
      }
      return 0;
    }
  } else if (type == "tcp") {
    bandwidth =
        RunTcpBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "uds") {
    bandwidth =
        RunUdsBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "pipe") {
    bandwidth =
        RunPipeBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "fifo") {
    bandwidth =
        RunFifoBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "mmap") {
    bandwidth =
        RunMmapBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "shm") {
    bandwidth =
        RunShmBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else {
    LOG(ERROR) << "Unknown benchmark type: " << type
               << ". Available types: memcpy, memcpy_mt, tcp, udp, uds, pipe, "
                  "fifo, mmap, shm, all";
    return 1;
  }

  // Print the result to stdout
  std::cout << type << ": " << bandwidth / (1ULL << 30) << GIBYTE_PER_SEC_UNIT
            << std::endl;

  return 0;
}
