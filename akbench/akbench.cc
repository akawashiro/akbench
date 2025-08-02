#include <iostream>
#include <map>
#include <optional>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"

#include "common.h"

// Latency benchmark headers
#include "atomic_latency.h"
#include "barrier_latency.h"
#include "condition_variable_latency.h"
#include "semaphore_latency.h"
#include "syscall_latency.h"

// Bandwidth benchmark headers
#include "fifo_bandwidth.h"
#include "memcpy_bandwidth.h"
#include "memcpy_mt_bandwidth.h"
#include "mmap_bandwidth.h"
#include "mq_bandwidth.h"
#include "pipe_bandwidth.h"
#include "shm_bandwidth.h"
#include "tcp_bandwidth.h"
#include "uds_bandwidth.h"

ABSL_FLAG(
    std::string, type, "",
    "Benchmark type to run:\n"
    "Latency tests: latency_atomic, latency_barrier, "
    "latency_condition_variable, "
    "latency_semaphore, latency_statfs, latency_fstatfs, latency_getpid, "
    "all_latency\n"
    "Bandwidth tests: bandwidth_memcpy, bandwidth_memcpy_mt, bandwidth_tcp, "
    "bandwidth_uds, bandwidth_pipe, bandwidth_fifo, bandwidth_mq, "
    "bandwidth_mmap, "
    "bandwidth_shm, all_bandwidth\n"
    "Combined: all");
ABSL_FLAG(int, num_iterations, 10,
          "Number of measurement iterations (minimum 3)");
ABSL_FLAG(int, num_warmups, 3, "Number of warmup iterations");
ABSL_FLAG(std::optional<uint64_t>, loop_size, std::nullopt,
          "Number of iterations in each measurement loop for latency tests. "
          "Use type-specific default if not specified.");
ABSL_FLAG(uint64_t, data_size, (1 << 30),
          "Size of data to transfer in bytes for bandwidth tests");
ABSL_FLAG(std::optional<uint64_t>, buffer_size, std::nullopt,
          "Buffer size for I/O operations in bytes for bandwidth tests "
          "(default: 1 MiByte, not applicable to memcpy benchmarks)");
ABSL_FLAG(std::optional<uint64_t>, num_threads, std::nullopt,
          "Number of threads for bandwidth_memcpy_mt benchmark (default: run "
          "with 1-4 "
          "threads)");
ABSL_FLAG(std::optional<int>, vlog, std::nullopt,
          "Show VLOG messages lower than this level.");
ABSL_FLAG(std::string, log_level, "WARNING",
          "Log level (INFO, DEBUG, WARNING, ERROR)");

constexpr uint64_t DEFAULT_BUFFER_SIZE = 1 << 20; // 1 MiByte

void RunLatencyBenchmarks(
    int num_iterations, int num_warmups,
    const std::map<std::string, uint64_t> &default_loop_sizes,
    const std::optional<uint64_t> &loop_size_opt, const std::string &type) {

  const uint64_t atomic_loop_size = loop_size_opt.has_value()
                                        ? *loop_size_opt
                                        : default_loop_sizes.at("atomic");
  const uint64_t barrier_loop_size = loop_size_opt.has_value()
                                         ? *loop_size_opt
                                         : default_loop_sizes.at("barrier");
  const uint64_t cv_loop_size =
      loop_size_opt.has_value() ? *loop_size_opt
                                : default_loop_sizes.at("condition_variable");
  const uint64_t semaphore_loop_size = loop_size_opt.has_value()
                                           ? *loop_size_opt
                                           : default_loop_sizes.at("semaphore");
  const uint64_t statfs_loop_size = loop_size_opt.has_value()
                                        ? *loop_size_opt
                                        : default_loop_sizes.at("statfs");
  const uint64_t fstatfs_loop_size = loop_size_opt.has_value()
                                         ? *loop_size_opt
                                         : default_loop_sizes.at("fstatfs");
  const uint64_t getpid_loop_size = loop_size_opt.has_value()
                                        ? *loop_size_opt
                                        : default_loop_sizes.at("getpid");

  double result = 0.0;

  if (type == "all_latency") {
    std::vector<std::pair<std::string, double>> results;

    result = RunAtomicLatencyBenchmark(num_iterations, num_warmups,
                                       atomic_loop_size);
    results.emplace_back("latency_atomic", result);

    result = RunBarrierLatencyBenchmark(num_iterations, num_warmups,
                                        barrier_loop_size);
    results.emplace_back("latency_barrier", result);

    result = RunConditionVariableLatencyBenchmark(num_iterations, num_warmups,
                                                  cv_loop_size);
    results.emplace_back("latency_condition_variable", result);

    result = RunSemaphoreLatencyBenchmark(num_iterations, num_warmups,
                                          semaphore_loop_size);
    results.emplace_back("latency_semaphore", result);

    result = RunStatfsLatencyBenchmark(num_iterations, num_warmups,
                                       statfs_loop_size);
    results.emplace_back("latency_statfs", result);

    result = RunFstatfsLatencyBenchmark(num_iterations, num_warmups,
                                        fstatfs_loop_size);
    results.emplace_back("latency_fstatfs", result);

    result = RunGetpidLatencyBenchmark(num_iterations, num_warmups,
                                       getpid_loop_size);
    results.emplace_back("latency_getpid", result);

    // Output all results at the end
    for (const auto &benchmark_result : results) {
      std::cout << benchmark_result.first << ": "
                << benchmark_result.second * 1e9 << " ns" << std::endl;
    }
  } else if (type == "latency_atomic") {
    result = RunAtomicLatencyBenchmark(num_iterations, num_warmups,
                                       atomic_loop_size);
    std::cout << "Atomic benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "latency_barrier") {
    result = RunBarrierLatencyBenchmark(num_iterations, num_warmups,
                                        barrier_loop_size);
    std::cout << "Barrier benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "latency_condition_variable") {
    result = RunConditionVariableLatencyBenchmark(num_iterations, num_warmups,
                                                  cv_loop_size);
    std::cout << "Condition Variable benchmark result: " << result * 1e9
              << " ns\n";
  } else if (type == "latency_semaphore") {
    result = RunSemaphoreLatencyBenchmark(num_iterations, num_warmups,
                                          semaphore_loop_size);
    std::cout << "Semaphore benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "latency_statfs") {
    result = RunStatfsLatencyBenchmark(num_iterations, num_warmups,
                                       statfs_loop_size);
    std::cout << "Statfs benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "latency_fstatfs") {
    result = RunFstatfsLatencyBenchmark(num_iterations, num_warmups,
                                        fstatfs_loop_size);
    std::cout << "Fstatfs benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "latency_getpid") {
    result = RunGetpidLatencyBenchmark(num_iterations, num_warmups,
                                       getpid_loop_size);
    std::cout << "Getpid benchmark result: " << result * 1e9 << " ns\n";
  }
}

void RunBandwidthBenchmarks(int num_iterations, int num_warmups,
                            uint64_t data_size, uint64_t buffer_size,
                            const std::optional<uint64_t> &num_threads_opt,
                            const std::string &type) {
  double bandwidth = 0.0;

  if (type == "all_bandwidth") {
    // Collect all benchmark results first, then output at the end
    std::vector<std::pair<std::string, double>> results;

    bandwidth =
        RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);
    results.emplace_back("bandwidth_memcpy", bandwidth);

    // Run memcpy_mt with 1-4 threads for "all_bandwidth" case
    for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
      bandwidth = RunMemcpyMtBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, n_threads);
      results.emplace_back("bandwidth_memcpy_mt (" + std::to_string(n_threads) +
                               " threads)",
                           bandwidth);
    }

    bandwidth = RunTcpBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    results.emplace_back("bandwidth_tcp", bandwidth);

    bandwidth = RunUdsBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    results.emplace_back("bandwidth_uds", bandwidth);

    bandwidth = RunPipeBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    results.emplace_back("bandwidth_pipe", bandwidth);

    bandwidth = RunFifoBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    results.emplace_back("bandwidth_fifo", bandwidth);

    bandwidth = RunMqBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                        buffer_size);
    results.emplace_back("bandwidth_mq", bandwidth);

    bandwidth = RunMmapBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    results.emplace_back("bandwidth_mmap", bandwidth);

    bandwidth = RunShmBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    results.emplace_back("bandwidth_shm", bandwidth);

    // Output all results at the end
    for (const auto &result : results) {
      std::cout << result.first << ": " << result.second / (1ULL << 30)
                << GIBYTE_PER_SEC_UNIT << std::endl;
    }
  } else if (type == "bandwidth_memcpy") {
    bandwidth =
        RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);
    std::cout << "bandwidth_memcpy: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  } else if (type == "bandwidth_memcpy_mt") {
    if (num_threads_opt.has_value()) {
      // Run with specified number of threads
      bandwidth = RunMemcpyMtBandwidthBenchmark(
          num_iterations, num_warmups, data_size, num_threads_opt.value());
      std::cout << "bandwidth_memcpy_mt: " << bandwidth / (1ULL << 30)
                << GIBYTE_PER_SEC_UNIT << std::endl;
    } else {
      // Run with 1-4 threads for compatibility
      for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
        bandwidth = RunMemcpyMtBandwidthBenchmark(num_iterations, num_warmups,
                                                  data_size, n_threads);
        std::cout << "bandwidth_memcpy_mt (" << n_threads
                  << " threads): " << bandwidth / (1ULL << 30)
                  << GIBYTE_PER_SEC_UNIT << std::endl;
      }
    }
  } else if (type == "bandwidth_tcp") {
    bandwidth = RunTcpBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    std::cout << "bandwidth_tcp: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  } else if (type == "bandwidth_uds") {
    bandwidth = RunUdsBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    std::cout << "bandwidth_uds: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  } else if (type == "bandwidth_pipe") {
    bandwidth = RunPipeBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    std::cout << "bandwidth_pipe: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  } else if (type == "bandwidth_fifo") {
    bandwidth = RunFifoBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    std::cout << "bandwidth_fifo: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  } else if (type == "bandwidth_mq") {
    bandwidth = RunMqBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                        buffer_size);
    std::cout << "bandwidth_mq: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  } else if (type == "bandwidth_mmap") {
    bandwidth = RunMmapBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    std::cout << "bandwidth_mmap: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  } else if (type == "bandwidth_shm") {
    bandwidth = RunShmBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    std::cout << "bandwidth_shm: " << bandwidth / (1ULL << 30)
              << GIBYTE_PER_SEC_UNIT << std::endl;
  }
}

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage(
      "Unified benchmark tool. Use --type to specify benchmark type.");
  absl::ParseCommandLine(argc, argv);

  const std::string type = absl::GetFlag(FLAGS_type);
  const int num_iterations = absl::GetFlag(FLAGS_num_iterations);
  const int num_warmups = absl::GetFlag(FLAGS_num_warmups);
  const std::optional<uint64_t> loop_size_opt = absl::GetFlag(FLAGS_loop_size);
  const uint64_t data_size = absl::GetFlag(FLAGS_data_size);
  const std::optional<uint64_t> buffer_size_opt =
      absl::GetFlag(FLAGS_buffer_size);
  const std::optional<uint64_t> num_threads_opt =
      absl::GetFlag(FLAGS_num_threads);

  if (type.empty()) {
    LOG(ERROR) << "Must specify --type. Available types:\n"
               << "Latency tests: latency_atomic, latency_barrier, "
                  "latency_condition_variable, "
               << "latency_semaphore, latency_statfs, latency_fstatfs, "
                  "latency_getpid, all_latency\n"
               << "Bandwidth tests: bandwidth_memcpy, bandwidth_memcpy_mt, "
                  "bandwidth_tcp, "
               << "bandwidth_uds, bandwidth_pipe, bandwidth_fifo, "
                  "bandwidth_mq, bandwidth_mmap, "
               << "bandwidth_shm, all_bandwidth\n"
               << "Combined: all";
    return 1;
  }

  if (num_iterations < 3) {
    LOG(ERROR) << "num_iterations must be at least 3, got: " << num_iterations;
    return 1;
  }

  // Check if buffer_size is specified for incompatible benchmark types
  if ((type == "bandwidth_memcpy" || type == "bandwidth_memcpy_mt") &&
      buffer_size_opt.has_value()) {
    LOG(ERROR) << "Buffer size option is not applicable to " << type
               << " benchmark type";
    return 1;
  }

  // Check if num_threads is specified for incompatible benchmark types
  if (type != "bandwidth_memcpy_mt" && num_threads_opt.has_value()) {
    LOG(ERROR)
        << "Number of threads option is only applicable to bandwidth_memcpy_mt "
           "benchmark type";
    return 1;
  }

  // Validate num_threads for memcpy_mt
  if (type == "bandwidth_memcpy_mt" && num_threads_opt.has_value() &&
      num_threads_opt.value() == 0) {
    LOG(ERROR) << "num_threads must be greater than 0, got: "
               << num_threads_opt.value();
    return 1;
  }

  // Get buffer size (use default if not specified)
  uint64_t buffer_size = buffer_size_opt.value_or(DEFAULT_BUFFER_SIZE);

  // Validate buffer_size for bandwidth tests
  if (type.find("bandwidth_") == 0 && type != "bandwidth_memcpy" &&
      type != "bandwidth_memcpy_mt") {
    if (buffer_size == 0) {
      LOG(ERROR) << "buffer_size must be greater than 0, got: " << buffer_size;
      return 1;
    }

    if (buffer_size > data_size) {
      LOG(ERROR) << "buffer_size (" << buffer_size
                 << ") cannot be larger than data_size (" << data_size << ")";
      return 1;
    }
  }

  // Validate data_size for bandwidth tests
  if (type.find("bandwidth_") == 0 || type == "all_bandwidth" ||
      type == "all") {
    if (data_size <= CHECKSUM_SIZE) {
      LOG(ERROR) << "data_size must be larger than CHECKSUM_SIZE ("
                 << CHECKSUM_SIZE << "), got: " << data_size;
      return 1;
    }
  }

  std::optional<int> vlog = absl::GetFlag(FLAGS_vlog);
  if (vlog.has_value()) {
    int v = *vlog;
    absl::SetGlobalVLogLevel(v);
  }

  // Set log level
  std::string log_level = absl::GetFlag(FLAGS_log_level);
  absl::LogSeverityAtLeast threshold = absl::LogSeverityAtLeast::kWarning;

  if (log_level == "INFO") {
    threshold = absl::LogSeverityAtLeast::kInfo;
  } else if (log_level == "DEBUG") {
    threshold =
        absl::LogSeverityAtLeast::kInfo; // Abseil uses INFO for DEBUG level
  } else if (log_level == "WARNING") {
    threshold = absl::LogSeverityAtLeast::kWarning;
  } else if (log_level == "ERROR") {
    threshold = absl::LogSeverityAtLeast::kError;
  } else {
    LOG(ERROR) << "Invalid log level: " << log_level
               << ". Available levels: INFO, DEBUG, WARNING, ERROR";
    return 1;
  }

  absl::SetStderrThreshold(threshold);
  absl::InitializeLog();

  // Define default loop sizes for latency tests
  const std::map<std::string, uint64_t> default_loop_sizes = {
      {"atomic", 1e6},    {"barrier", 1e3}, {"condition_variable", 1e5},
      {"semaphore", 1e5}, {"statfs", 1e6},  {"fstatfs", 1e6},
      {"getpid", 1e6}};

  // Handle the "all" case which runs all tests
  if (type == "all") {
    std::cout << "Running all latency tests:\n" << std::endl;
    RunLatencyBenchmarks(num_iterations, num_warmups, default_loop_sizes,
                         loop_size_opt, "all_latency");

    std::cout << "\nRunning all bandwidth tests:\n" << std::endl;
    RunBandwidthBenchmarks(num_iterations, num_warmups, data_size, buffer_size,
                           num_threads_opt, "all_bandwidth");
    return 0;
  }

  // Handle latency tests
  if (type.find("latency_") == 0 || type == "all_latency") {
    RunLatencyBenchmarks(num_iterations, num_warmups, default_loop_sizes,
                         loop_size_opt, type);
  }
  // Handle bandwidth tests
  else if (type.find("bandwidth_") == 0 || type == "all_bandwidth") {
    RunBandwidthBenchmarks(num_iterations, num_warmups, data_size, buffer_size,
                           num_threads_opt, type);
  } else {
    LOG(ERROR) << "Unknown benchmark type: " << type << ". Available types:\n"
               << "Latency tests: latency_atomic, latency_barrier, "
                  "latency_condition_variable, "
               << "latency_semaphore, latency_statfs, latency_fstatfs, "
                  "latency_getpid, all_latency\n"
               << "Bandwidth tests: bandwidth_memcpy, bandwidth_memcpy_mt, "
                  "bandwidth_tcp, "
               << "bandwidth_uds, bandwidth_pipe, bandwidth_fifo, "
                  "bandwidth_mq, bandwidth_mmap, "
               << "bandwidth_shm, all_bandwidth\n"
               << "Combined: all";
    return 1;
  }

  return 0;
}
