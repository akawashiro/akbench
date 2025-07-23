#include <optional>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"

#include "common.h"
#include "memcpy_benchmark.h"
#include "memcpy_mt_benchmark.h"
#include "mmap_benchmark.h"
#include "pipe_benchmark.h"
#include "shm_benchmark.h"
#include "tcp_benchmark.h"
#include "uds_benchmark.h"

ABSL_FLAG(std::string, type, "",
          "Benchmark type to run (memcpy, memcpy_mt, tcp, uds, pipe, "
          "mmap, shm)");
ABSL_FLAG(int, num_iterations, 10,
          "Number of measurement iterations (minimum 3)");
ABSL_FLAG(int, num_warmups, 3, "Number of warmup iterations");
ABSL_FLAG(uint64_t, data_size, (1 << 30),
          "Size of data to transfer in bytes");
ABSL_FLAG(std::optional<uint64_t>, buffer_size, std::nullopt,
          "Buffer size for I/O operations in bytes (default: 1 MiByte, not "
          "applicable to memcpy benchmarks)");
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

  // Validate type
  if (type.empty()) {
    LOG(ERROR) << "Must specify --type. Available types: memcpy, memcpy_mt, "
                  "tcp, uds, pipe, mmap, shm, all";
    return 1;
  }

  // Check if buffer_size is specified for incompatible benchmark types
  if ((type == "memcpy" || type == "memcpy_mt") &&
      buffer_size_opt.has_value()) {
    LOG(ERROR) << "Buffer size option is not applicable to " << type
               << " benchmark type";
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
  int result = 0;
  if (type == "memcpy") {
    result = RunMemcpyBenchmark(num_iterations, num_warmups, data_size);
  } else if (type == "memcpy_mt") {
    result = RunMemcpyMtBenchmark(num_iterations, num_warmups, data_size);
  } else if (type == "tcp") {
    result =
        RunTcpBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "uds") {
    result =
        RunUdsBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "pipe") {
    result =
        RunPipeBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "mmap") {
    result =
        RunMmapBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "shm") {
    result =
        RunShmBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else if (type == "all") {
    result = RunMemcpyBenchmark(num_iterations, num_warmups, data_size);
    result |= RunMemcpyMtBenchmark(num_iterations, num_warmups, data_size);
    result |=
        RunTcpBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    result |=
        RunUdsBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    result |=
        RunPipeBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    result |=
        RunMmapBenchmark(num_iterations, num_warmups, data_size, buffer_size);
    result |=
        RunShmBenchmark(num_iterations, num_warmups, data_size, buffer_size);
  } else {
    LOG(ERROR) << "Unknown benchmark type: " << type
               << ". Available types: memcpy, memcpy_mt, tcp, udp, uds, pipe, "
                  "mmap, shm, all";
    return 1;
  }

  return result;
}
