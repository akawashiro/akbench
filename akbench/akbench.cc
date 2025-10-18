#include <cstdlib>
#include <format>
#include <getopt.h>
#include <iostream>
#include <map>
#include <optional>
#include <print>
#include <vector>

#include "aklog.h"
#include "common.h"
#include "getopt_utils.h"

// Latency benchmark headers
#include "atomic_latency.h"
#include "atomic_rel_acq_latency.h"
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

// Command line option variables
static std::string g_type = "";
static int g_num_iterations = 10;
static int g_num_warmups = 3;
static std::optional<uint64_t> g_loop_size = std::nullopt;
static uint64_t g_data_size = (1ULL << 30); // 1GB default
static std::optional<uint64_t> g_buffer_size = std::nullopt;
static std::optional<uint64_t> g_num_threads = std::nullopt;
static std::string g_log_level = "WARNING";
static bool g_json_output = false;

constexpr uint64_t DEFAULT_BUFFER_SIZE = 1 << 20; // 1 MiByte

void PrintUsage(const char *program_name) {
  std::cout << R"(Usage: )" << program_name << R"( <TYPE> [OPTIONS]

Unified benchmark tool for measuring system performance.

Arguments:
  TYPE                         Benchmark type to run (required)

Latency Tests (measure operation latency in nanoseconds):
  latency_atomic               Atomic variable synchronization between threads
  latency_atomic_rel_acq       Atomic operations with relaxed-acquire memory ordering
  latency_barrier              Barrier between process synchronization.
                               We use this barrier in bandwidth tests.
  latency_condition_variable   Condition variable wait/notify operations
  latency_semaphore            Semaphore wait/post operations
  latency_statfs               statfs() filesystem syscall
  latency_fstatfs              fstatfs() filesystem syscall
  latency_getpid               getpid() syscall
  latency_all                  Run all latency benchmarks

Bandwidth Tests (measure data transfer rate in GiByte/sec):
  bandwidth_memcpy             Memory copy using memcpy()
  bandwidth_memcpy_mt          Multi-threaded memory copy
  bandwidth_tcp                TCP socket communication
  bandwidth_uds                Unix domain socket communication
  bandwidth_pipe               Anonymous pipe communication
  bandwidth_fifo               Named pipe (FIFO) communication
  bandwidth_mq                 POSIX message queue communication
  bandwidth_mmap               Memory-mapped file communication
                               Use double buffering.
  bandwidth_shm                Shared memory communication.
                               Use double buffering.
  bandwidth_all                Run all bandwidth benchmarks

Combined:
  all                          Run all latency and bandwidth benchmarks

Options:
  -i, --num-iterations=N       Number of measurement iterations (default: 10)
  -w, --num-warmups=N          Number of warmup iterations (default: 3)
  -l, --loop-size=N            Loop size for latency tests
                               The default value varies depending on the test.
  -d, --data-size=SIZE         Data size in bytes for bandwidth tests (default: 1GB)
  -b, --buffer-size=SIZE       Buffer size in bytes for I/O operations (default: 1MB)
                               Not applicable to memcpy benchmarks
  -n, --num-threads=N          Number of threads for bandwidth_memcpy_mt
  --log-level=LEVEL            Log level: INFO, DEBUG, WARNING, ERROR (default: WARNING)
  --json-output                Output results in JSON format
  -h, --help                   Display this help message
)";
}

// Helper function to output a single benchmark result as JSON
void OutputJsonResult(const std::string &name, const BenchmarkResult &result,
                      const std::string &unit) {
  std::println(R"({{)");
  std::println(R"(  "name": "{}",)", name);
  std::println(R"(  "average": {:e},)", result.average);
  std::println(R"(  "stddev": {:e},)", result.stddev);
  std::println(R"(  "unit": "{}")", unit);
  std::println(R"(}})");
}

// Helper function to output multiple benchmark results as JSON
void OutputJsonResults(
    const std::vector<std::pair<std::string, BenchmarkResult>> &results,
    const std::string &unit) {
  std::println("[");
  for (size_t i = 0; i < results.size(); ++i) {
    const auto &[name, result] = results[i];
    std::println("  {{");
    std::println(R"(    "name": "{}",)", name);
    std::println(R"(    "average": {:e},)", result.average);
    std::println(R"(    "stddev": {:e},)", result.stddev);
    std::println(R"(    "unit": "{}")", unit);
    if (i < results.size() - 1) {
      std::println("  }},");
    } else {
      std::println("  }}");
    }
  }
  std::println("]");
}

// Helper function to output latency and bandwidth results as a JSON dictionary
void OutputJsonDictionary(
    const std::vector<std::pair<std::string, BenchmarkResult>> &latency_results,
    const std::vector<std::pair<std::string, BenchmarkResult>>
        &bandwidth_results) {
  std::println("{{");

  // Output latency array
  std::println(R"(  "latency": [)");
  for (size_t i = 0; i < latency_results.size(); ++i) {
    const auto &[name, result] = latency_results[i];
    std::println("    {{");
    std::println(R"(      "name": "{}",)", name);
    std::println(R"(      "average": {:e},)", result.average);
    std::println(R"(      "stddev": {:e},)", result.stddev);
    std::println(R"(      "unit": "sec")");
    if (i < latency_results.size() - 1) {
      std::println("    }},");
    } else {
      std::println("    }}");
    }
  }
  std::println("  ],");

  // Output bandwidth array
  std::println(R"(  "bandwidth": [)");
  for (size_t i = 0; i < bandwidth_results.size(); ++i) {
    const auto &[name, result] = bandwidth_results[i];
    std::println("    {{");
    std::println(R"(      "name": "{}",)", name);
    std::println(R"(      "average": {:e},)", result.average);
    std::println(R"(      "stddev": {:e},)", result.stddev);
    std::println(R"(      "unit": "Byte/sec")");
    if (i < bandwidth_results.size() - 1) {
      std::println("    }},");
    } else {
      std::println("    }}");
    }
  }
  std::println("  ]");

  std::println("}}");
}

// Helper function to output latency results
void OutputLatencyResults(const std::map<std::string, BenchmarkResult> &results,
                          bool json_output) {
  if (results.empty()) {
    return;
  }

  if (json_output) {
    // Convert map to vector for OutputJsonResults
    std::vector<std::pair<std::string, BenchmarkResult>> results_vec(
        results.begin(), results.end());
    OutputJsonResults(results_vec, "sec");
  } else {
    for (const auto &[name, result] : results) {
      std::println("{}: {:.3f} ± {:.3f} ns", name, result.average * 1e9,
                   result.stddev * 1e9);
    }
  }
}

// Helper function to output bandwidth results
void OutputBandwidthResults(
    const std::map<std::string, BenchmarkResult> &results, bool json_output) {
  if (results.empty()) {
    return;
  }

  if (json_output) {
    // Convert map to vector for OutputJsonResults
    std::vector<std::pair<std::string, BenchmarkResult>> results_vec(
        results.begin(), results.end());
    OutputJsonResults(results_vec, "Byte/sec");
  } else {
    for (const auto &[name, result] : results) {
      std::println("{}: {:.3f} ± {:.3f}{}", name, result.average / (1ULL << 30),
                   result.stddev / (1ULL << 30), GIBYTE_PER_SEC_UNIT);
    }
  }
}

std::map<std::string, BenchmarkResult>
RunLatencyBenchmarks(int num_iterations, int num_warmups,
                     const std::map<std::string, uint64_t> &default_loop_sizes,
                     const std::optional<uint64_t> &loop_size_opt,
                     const std::string &type) {

  const uint64_t atomic_loop_size = loop_size_opt.has_value()
                                        ? *loop_size_opt
                                        : default_loop_sizes.at("atomic");
  const uint64_t atomic_rel_acq_loop_size =
      loop_size_opt.has_value() ? *loop_size_opt
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

  std::map<std::string, BenchmarkResult> results;
  BenchmarkResult result;

  if (type == "latency_all") {
    result = RunAtomicLatencyBenchmark(num_iterations, num_warmups,
                                       atomic_loop_size);
    results["latency_atomic"] = result;

    result = RunAtomicRelAcqLatencyBenchmark(num_iterations, num_warmups,
                                             atomic_rel_acq_loop_size);
    results["latency_atomic_rel_acq"] = result;

    result = RunBarrierLatencyBenchmark(num_iterations, num_warmups,
                                        barrier_loop_size);
    results["latency_barrier"] = result;

    result = RunConditionVariableLatencyBenchmark(num_iterations, num_warmups,
                                                  cv_loop_size);
    results["latency_condition_variable"] = result;

    result = RunSemaphoreLatencyBenchmark(num_iterations, num_warmups,
                                          semaphore_loop_size);
    results["latency_semaphore"] = result;

    result = RunStatfsLatencyBenchmark(num_iterations, num_warmups,
                                       statfs_loop_size);
    results["latency_statfs"] = result;

    result = RunFstatfsLatencyBenchmark(num_iterations, num_warmups,
                                        fstatfs_loop_size);
    results["latency_fstatfs"] = result;

    result = RunGetpidLatencyBenchmark(num_iterations, num_warmups,
                                       getpid_loop_size);
    results["latency_getpid"] = result;
  } else if (type == "latency_atomic") {
    result = RunAtomicLatencyBenchmark(num_iterations, num_warmups,
                                       atomic_loop_size);
    results["latency_atomic"] = result;
  } else if (type == "latency_atomic_rel_acq") {
    result = RunAtomicRelAcqLatencyBenchmark(num_iterations, num_warmups,
                                             atomic_rel_acq_loop_size);
    results["latency_atomic_rel_acq"] = result;
  } else if (type == "latency_barrier") {
    result = RunBarrierLatencyBenchmark(num_iterations, num_warmups,
                                        barrier_loop_size);
    results["latency_barrier"] = result;
  } else if (type == "latency_condition_variable") {
    result = RunConditionVariableLatencyBenchmark(num_iterations, num_warmups,
                                                  cv_loop_size);
    results["latency_condition_variable"] = result;
  } else if (type == "latency_semaphore") {
    result = RunSemaphoreLatencyBenchmark(num_iterations, num_warmups,
                                          semaphore_loop_size);
    results["latency_semaphore"] = result;
  } else if (type == "latency_statfs") {
    result = RunStatfsLatencyBenchmark(num_iterations, num_warmups,
                                       statfs_loop_size);
    results["latency_statfs"] = result;
  } else if (type == "latency_fstatfs") {
    result = RunFstatfsLatencyBenchmark(num_iterations, num_warmups,
                                        fstatfs_loop_size);
    results["latency_fstatfs"] = result;
  } else if (type == "latency_getpid") {
    result = RunGetpidLatencyBenchmark(num_iterations, num_warmups,
                                       getpid_loop_size);
    results["latency_getpid"] = result;
  }

  return results;
}

std::map<std::string, BenchmarkResult>
RunBandwidthBenchmarks(int num_iterations, int num_warmups, uint64_t data_size,
                       uint64_t buffer_size,
                       const std::optional<uint64_t> &num_threads_opt,
                       const std::string &type) {
  std::map<std::string, BenchmarkResult> results;
  BenchmarkResult benchmark_result;

  if (type == "bandwidth_all") {
    benchmark_result =
        RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);
    results["bandwidth_memcpy"] = benchmark_result;

    // Run memcpy_mt with 1-4 threads for "bandwidth_all" case
    for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
      benchmark_result = RunMemcpyMtBandwidthBenchmark(
          num_iterations, num_warmups, data_size, n_threads);
      results["bandwidth_memcpy_mt (" + std::to_string(n_threads) +
              " threads)"] = benchmark_result;
    }

    benchmark_result = RunTcpBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, buffer_size);
    results["bandwidth_tcp"] = benchmark_result;

    benchmark_result = RunUdsBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, buffer_size);
    results["bandwidth_uds"] = benchmark_result;

    benchmark_result = RunPipeBandwidthBenchmark(num_iterations, num_warmups,
                                                 data_size, buffer_size);
    results["bandwidth_pipe"] = benchmark_result;

    benchmark_result = RunFifoBandwidthBenchmark(num_iterations, num_warmups,
                                                 data_size, buffer_size);
    results["bandwidth_fifo"] = benchmark_result;

    benchmark_result = RunMqBandwidthBenchmark(num_iterations, num_warmups,
                                               data_size, buffer_size);
    results["bandwidth_mq"] = benchmark_result;

    benchmark_result = RunMmapBandwidthBenchmark(num_iterations, num_warmups,
                                                 data_size, buffer_size);
    results["bandwidth_mmap"] = benchmark_result;

    benchmark_result = RunShmBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, buffer_size);
    results["bandwidth_shm"] = benchmark_result;
  } else if (type == "bandwidth_memcpy") {
    benchmark_result =
        RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);
    results["bandwidth_memcpy"] = benchmark_result;
  } else if (type == "bandwidth_memcpy_mt") {
    if (num_threads_opt.has_value()) {
      // Run with specified number of threads
      benchmark_result = RunMemcpyMtBandwidthBenchmark(
          num_iterations, num_warmups, data_size, num_threads_opt.value());
      results["bandwidth_memcpy_mt"] = benchmark_result;
    } else {
      // Run with 1-4 threads for compatibility
      for (uint64_t n_threads = 1; n_threads <= 4; ++n_threads) {
        benchmark_result = RunMemcpyMtBandwidthBenchmark(
            num_iterations, num_warmups, data_size, n_threads);
        results["bandwidth_memcpy_mt (" + std::to_string(n_threads) +
                " threads)"] = benchmark_result;
      }
    }
  } else if (type == "bandwidth_tcp") {
    benchmark_result = RunTcpBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, buffer_size);
    results["bandwidth_tcp"] = benchmark_result;
  } else if (type == "bandwidth_uds") {
    benchmark_result = RunUdsBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, buffer_size);
    results["bandwidth_uds"] = benchmark_result;
  } else if (type == "bandwidth_pipe") {
    benchmark_result = RunPipeBandwidthBenchmark(num_iterations, num_warmups,
                                                 data_size, buffer_size);
    results["bandwidth_pipe"] = benchmark_result;
  } else if (type == "bandwidth_fifo") {
    benchmark_result = RunFifoBandwidthBenchmark(num_iterations, num_warmups,
                                                 data_size, buffer_size);
    results["bandwidth_fifo"] = benchmark_result;
  } else if (type == "bandwidth_mq") {
    benchmark_result = RunMqBandwidthBenchmark(num_iterations, num_warmups,
                                               data_size, buffer_size);
    results["bandwidth_mq"] = benchmark_result;
  } else if (type == "bandwidth_mmap") {
    benchmark_result = RunMmapBandwidthBenchmark(num_iterations, num_warmups,
                                                 data_size, buffer_size);
    results["bandwidth_mmap"] = benchmark_result;
  } else if (type == "bandwidth_shm") {
    benchmark_result = RunShmBandwidthBenchmark(num_iterations, num_warmups,
                                                data_size, buffer_size);
    results["bandwidth_shm"] = benchmark_result;
  }

  return results;
}

int main(int argc, char *argv[]) {
  const char *program_name = argv[0];

  // Define long options
  static struct option long_options[] = {
      {"num-iterations", required_argument, nullptr, 'i'},
      {"num-warmups", required_argument, nullptr, 'w'},
      {"loop-size", required_argument, nullptr, 'l'},
      {"data-size", required_argument, nullptr, 'd'},
      {"buffer-size", required_argument, nullptr, 'b'},
      {"num-threads", required_argument, nullptr, 'n'},
      {"log-level", required_argument, nullptr, 256}, // No short option
      {"json-output", no_argument, nullptr, 257},     // No short option
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  // Parse command line options
  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "i:w:l:d:b:n:h", long_options,
                            &option_index)) != -1) {
    try {
      switch (opt) {
      case 'i':
        g_num_iterations = ParseInt(optarg);
        break;
      case 'w':
        g_num_warmups = ParseInt(optarg);
        break;
      case 'l':
        g_loop_size = ParseUint64(optarg);
        break;
      case 'd':
        g_data_size = ParseUint64(optarg).value();
        break;
      case 'b':
        g_buffer_size = ParseUint64(optarg);
        break;
      case 'n':
        g_num_threads = ParseUint64(optarg);
        break;
      case 256: // --log-level
        g_log_level = optarg;
        break;
      case 257: // --json-output
        g_json_output = true;
        break;
      case 'h':
        PrintUsage(program_name);
        return 0;
      case '?':
        // getopt_long already printed an error message
        return 1;
      default:
        PrintErrorAndExit(program_name, "Unknown option");
      }
    } catch (const std::exception &e) {
      PrintErrorAndExit(program_name, e.what());
    }
  }

  // Check for exactly one positional argument (the benchmark type)
  if (optind >= argc) {
    PrintErrorAndExit(program_name, "Missing required argument: TYPE");
  }

  if (optind + 1 < argc) {
    PrintErrorAndExit(program_name,
                      "Too many arguments. Expected only TYPE, got: " +
                          std::string(argv[optind + 1]));
  }

  // Get the benchmark type from the positional argument
  g_type = argv[optind];

  // Use parsed values
  const std::string &type = g_type;
  const int num_iterations = g_num_iterations;
  const int num_warmups = g_num_warmups;
  const std::optional<uint64_t> &loop_size_opt = g_loop_size;
  const uint64_t data_size = g_data_size;
  const std::optional<uint64_t> &buffer_size_opt = g_buffer_size;
  const std::optional<uint64_t> &num_threads_opt = g_num_threads;

  if (type.empty()) {
    AKLOG(
        aklog::LogLevel::ERROR,
        "Must specify TYPE as first argument. Available types:\nLatency tests: "
        "latency_atomic, latency_atomic_rel_acq, latency_barrier, "
        "latency_condition_variable, "
        "latency_semaphore, latency_statfs, latency_fstatfs, latency_getpid, "
        "latency_all\nBandwidth tests: bandwidth_memcpy, "
        "bandwidth_memcpy_mt, bandwidth_tcp, bandwidth_uds, bandwidth_pipe, "
        "bandwidth_fifo, bandwidth_mq, bandwidth_mmap, bandwidth_shm, "
        "bandwidth_all\nCombined: all");
    return 1;
  }

  if (num_iterations < 3) {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("num_iterations must be at least 3, got: {}",
                      num_iterations));
    return 1;
  }

  // Check if buffer_size is specified for incompatible benchmark types
  if ((type == "bandwidth_memcpy" || type == "bandwidth_memcpy_mt") &&
      buffer_size_opt.has_value()) {
    AKLOG(
        aklog::LogLevel::ERROR,
        std::format("Buffer size option is not applicable to {} benchmark type",
                    type));
    return 1;
  }

  // Check if num_threads is specified for incompatible benchmark types
  if (type != "bandwidth_memcpy_mt" && num_threads_opt.has_value()) {
    AKLOG(aklog::LogLevel::ERROR, "Number of threads option is only applicable "
                                  "to bandwidth_memcpy_mt benchmark type");
    return 1;
  }

  // Validate num_threads for memcpy_mt
  if (type == "bandwidth_memcpy_mt" && num_threads_opt.has_value() &&
      num_threads_opt.value() == 0) {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("num_threads must be greater than 0, got: {}",
                      num_threads_opt.value()));
    return 1;
  }

  // Get buffer size (use default if not specified)
  uint64_t buffer_size = buffer_size_opt.value_or(DEFAULT_BUFFER_SIZE);

  // Validate buffer_size for bandwidth tests
  if (type.find("bandwidth_") == 0 && type != "bandwidth_memcpy" &&
      type != "bandwidth_memcpy_mt") {
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
  }

  // Validate data_size for bandwidth tests
  if (type.find("bandwidth_") == 0 || type == "bandwidth_all" ||
      type == "all") {
    if (data_size <= CHECKSUM_SIZE) {
      AKLOG(aklog::LogLevel::ERROR,
            std::format(
                "data_size must be larger than CHECKSUM_SIZE ({}), got: {}",
                CHECKSUM_SIZE, data_size));
      return 1;
    }
  }

  // Set log level
  const std::string &log_level = g_log_level;

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

  // Define default loop sizes for latency tests
  const std::map<std::string, uint64_t> default_loop_sizes = {
      {"atomic", 1e6},    {"barrier", 1e3}, {"condition_variable", 1e5},
      {"semaphore", 1e5}, {"statfs", 1e6},  {"fstatfs", 1e6},
      {"getpid", 1e6}};

  // Handle the "all" case which runs all tests
  if (type == "all") {
    // Run all latency benchmarks
    auto latency_results =
        RunLatencyBenchmarks(num_iterations, num_warmups, default_loop_sizes,
                             loop_size_opt, "latency_all");

    // Run all bandwidth benchmarks
    auto bandwidth_results =
        RunBandwidthBenchmarks(num_iterations, num_warmups, data_size,
                               buffer_size, num_threads_opt, "bandwidth_all");

    if (g_json_output) {
      // For JSON output, output as a dictionary
      std::vector<std::pair<std::string, BenchmarkResult>> latency_vec(
          latency_results.begin(), latency_results.end());
      std::vector<std::pair<std::string, BenchmarkResult>> bandwidth_vec(
          bandwidth_results.begin(), bandwidth_results.end());
      OutputJsonDictionary(latency_vec, bandwidth_vec);
    } else {
      // For non-JSON output
      std::println("Running all latency tests:");
      std::println("");
      OutputLatencyResults(latency_results, g_json_output);

      std::println("");
      std::println("Running all bandwidth tests:");
      std::println("");
      OutputBandwidthResults(bandwidth_results, g_json_output);
    }
    return 0;
  }

  // Handle latency tests
  if (type.find("latency_") == 0 || type == "latency_all") {
    auto results = RunLatencyBenchmarks(
        num_iterations, num_warmups, default_loop_sizes, loop_size_opt, type);
    OutputLatencyResults(results, g_json_output);
  }
  // Handle bandwidth tests
  else if (type.find("bandwidth_") == 0 || type == "bandwidth_all") {
    auto results =
        RunBandwidthBenchmarks(num_iterations, num_warmups, data_size,
                               buffer_size, num_threads_opt, type);
    OutputBandwidthResults(results, g_json_output);
  } else {
    AKLOG(aklog::LogLevel::ERROR,
          std::format(
              "Unknown benchmark type: {}. Available types:\nLatency tests: "
              "latency_atomic, latency_atomic_rel_acq, latency_barrier, "
              "latency_condition_variable, "
              "latency_semaphore, latency_statfs, latency_fstatfs, "
              "latency_getpid, latency_all\nBandwidth tests: bandwidth_memcpy, "
              "bandwidth_memcpy_mt, bandwidth_tcp, bandwidth_uds, "
              "bandwidth_pipe, bandwidth_fifo, bandwidth_mq, bandwidth_mmap, "
              "bandwidth_shm, bandwidth_all\nCombined: all",
              type));
    return 1;
  }

  return 0;
}
