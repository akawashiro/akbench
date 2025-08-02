#include <format>
#include <optional>
#include <print>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "aklog.h"

#include "common.h"
#include "fifo_bandwidth.h"
#include "memcpy_bandwidth.h"
#include "memcpy_mt_bandwidth.h"
#include "mmap_bandwidth.h"
#include "mq_bandwidth.h"
#include "pipe_bandwidth.h"
#include "shm_bandwidth.h"
#include "tcp_bandwidth.h"
#include "uds_bandwidth.h"

ABSL_FLAG(std::string, type, "",
          "Benchmark type to run (memcpy, memcpy_mt, tcp, uds, pipe, fifo, "
          "mq, mmap, shm)");
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
ABSL_FLAG(std::string, log_level, "WARNING",
          "Log level (INFO, DEBUG, WARNING, ERROR)");

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
    AKLOG(aklog::LogLevel::ERROR,
          "Must specify --type. Available types: memcpy, memcpy_mt, tcp, uds, "
          "pipe, fifo, mmap, shm, all");
    return 1;
  }

  // Check if buffer_size is specified for incompatible benchmark types
  if ((type == "memcpy" || type == "memcpy_mt") &&
      buffer_size_opt.has_value()) {
    AKLOG(
        aklog::LogLevel::ERROR,
        std::format("Buffer size option is not applicable to {} benchmark type",
                    type));
    return 1;
  }

  // Check if num_threads is specified for incompatible benchmark types
  if (type != "memcpy_mt" && num_threads_opt.has_value()) {
    AKLOG(aklog::LogLevel::ERROR, "Number of threads option is only applicable "
                                  "to memcpy_mt benchmark type");
    return 1;
  }

  // Validate num_threads for memcpy_mt
  if (type == "memcpy_mt" && num_threads_opt.has_value() &&
      num_threads_opt.value() == 0) {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("num_threads must be greater than 0, got: {}",
                      num_threads_opt.value()));
    return 1;
  }

  // Get buffer size (use default if not specified)
  uint64_t buffer_size = buffer_size_opt.value_or(DEFAULT_BUFFER_SIZE);

  // Validate buffer_size
  if (buffer_size == 0) {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("buffer_size must be greater than 0, got: {}",
                      buffer_size));
    return 1;
  }

  if (buffer_size > data_size) {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("buffer_size ({}) cannot be larger than data_size ({})",
                      buffer_size, data_size));
    return 1;
  }

  // Validate num_iterations
  if (num_iterations < 3) {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("num_iterations must be at least 3, got: {}",
                      num_iterations));
    return 1;
  }

  // Validate data_size
  if (data_size <= CHECKSUM_SIZE) {
    AKLOG(
        aklog::LogLevel::ERROR,
        std::format("data_size must be larger than CHECKSUM_SIZE ({}), got: {}",
                    CHECKSUM_SIZE, data_size));
    return 1;
  }

  std::string log_level = absl::GetFlag(FLAGS_log_level);

  if (log_level == "INFO") {
    aklog::setLogLevel(aklog::LogLevel::INFO);
  } else if (log_level == "DEBUG") {
    aklog::setLogLevel(aklog::LogLevel::DEBUG);
  } else if (log_level == "WARNING") {
    aklog::setLogLevel(aklog::LogLevel::WARNING);
  } else if (log_level == "ERROR") {
    aklog::setLogLevel(aklog::LogLevel::ERROR);
  } else {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("Invalid log level: {}. Available levels: INFO, DEBUG, "
                      "WARNING, ERROR",
                      log_level));
    return 1;
  }

  // Run the appropriate benchmark
  double bandwidth = 0.0;

  if (type == "all") {
    // Collect all benchmark results first, then output at the end
    std::vector<std::pair<std::string, double>> results;

    bandwidth =
        RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);
    results.emplace_back("memcpy", bandwidth);

    // Run memcpy_mt with 1-4 threads for "all" case
    for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
      bandwidth = RunMemcpyMtBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, n_threads);
      results.emplace_back(
          "memcpy_mt (" + std::to_string(n_threads) + " threads)", bandwidth);
    }

    bandwidth = RunTcpBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    results.emplace_back("tcp", bandwidth);

    bandwidth = RunUdsBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    results.emplace_back("uds", bandwidth);

    bandwidth = RunPipeBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    results.emplace_back("pipe", bandwidth);

    bandwidth = RunFifoBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    results.emplace_back("fifo", bandwidth);

    bandwidth = RunMqBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                        buffer_size);
    results.emplace_back("mq", bandwidth);

    bandwidth = RunMmapBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
    results.emplace_back("mmap", bandwidth);

    bandwidth = RunShmBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
    results.emplace_back("shm", bandwidth);

    // Output all results at the end
    for (const auto &result : results) {
      std::println("{}: {}{}", result.first, result.second / (1ULL << 30), GIBYTE_PER_SEC_UNIT);
    }

    return 0;
  } else if (type == "memcpy") {
    bandwidth =
        RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);
  } else if (type == "memcpy_mt") {
    if (num_threads_opt.has_value()) {
      // Run with specified number of threads
      bandwidth = RunMemcpyMtBandwidthBenchmark(
          num_iterations, num_warmups, data_size, num_threads_opt.value());
    } else {
      // Run with 1-4 threads for compatibility
      for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
        bandwidth = RunMemcpyMtBandwidthBenchmark(num_iterations, num_warmups,
                                                  data_size, n_threads);
        std::println("memcpy_mt ({} threads): {}{}", n_threads, bandwidth / (1ULL << 30), GIBYTE_PER_SEC_UNIT);
      }
      return 0;
    }
  } else if (type == "tcp") {
    bandwidth = RunTcpBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
  } else if (type == "uds") {
    bandwidth = RunUdsBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
  } else if (type == "pipe") {
    bandwidth = RunPipeBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
  } else if (type == "fifo") {
    bandwidth = RunFifoBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
  } else if (type == "mq") {
    bandwidth = RunMqBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                        buffer_size);
  } else if (type == "mmap") {
    bandwidth = RunMmapBandwidthBenchmark(num_iterations, num_warmups,
                                          data_size, buffer_size);
  } else if (type == "shm") {
    bandwidth = RunShmBandwidthBenchmark(num_iterations, num_warmups, data_size,
                                         buffer_size);
  } else {
    AKLOG(
        aklog::LogLevel::ERROR,
        std::format("Unknown benchmark type: {}. Available types: memcpy, "
                    "memcpy_mt, tcp, udp, uds, pipe, fifo, mq, mmap, shm, all",
                    type));
    return 1;
  }

  // Print the result to stdout
  std::println("{}: {}{}", type, bandwidth / (1ULL << 30), GIBYTE_PER_SEC_UNIT);

  return 0;
}
