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

#include "atomic_latency.h"
#include "condition_variable_latency.h"
#include "semaphore_latency.h"
#include "syscall_latency.h"

ABSL_FLAG(std::string, type, "",
          "Benchmark type to run (atomic, condition_variable, semaphore, "
          "statfs, fstatfs, "
          "getpid, all)");
ABSL_FLAG(int, num_iterations, 10,
          "Number of measurement iterations (minimum 3)");
ABSL_FLAG(int, num_warmups, 3, "Number of warmup iterations");
ABSL_FLAG(std::optional<uint64_t>, loop_size, std::nullopt,
          "number of iterations in each "
          "measurement loop. Use type-specific default if not specified.");
ABSL_FLAG(std::optional<int>, vlog, std::nullopt,
          "Show VLOG messages lower than this level.");

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage(
      "Latency benchmark tool. Use --type to specify benchmark type.");
  absl::ParseCommandLine(argc, argv);

  const std::string type = absl::GetFlag(FLAGS_type);
  const int num_iterations = absl::GetFlag(FLAGS_num_iterations);
  const int num_warmups = absl::GetFlag(FLAGS_num_warmups);
  const std::optional<uint64_t> loop_size_opt = absl::GetFlag(FLAGS_loop_size);

  const std::map<std::string, uint64_t> default_loop_sizes = {
      {"atomic", 1e6},    {"condition_variable", 1e5},
      {"semaphore", 1e5}, {"statfs", 1e6},
      {"fstatfs", 1e6},   {"getpid", 1e6}};

  const uint64_t atomic_loop_size = loop_size_opt.has_value()
                                        ? *loop_size_opt
                                        : default_loop_sizes.at("atomic");
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

  if (type.empty()) {
    LOG(ERROR) << "Must specify --type. Available types: atomic, "
                  "condition_variable, semaphore, statfs, fstatfs, getpid, all";
    return 1;
  }

  if (num_iterations < 3) {
    LOG(ERROR) << "num_iterations must be at least 3, got: " << num_iterations;
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
  double result = 0.0;

  if (type == "all") {
    std::vector<std::pair<std::string, double>> results;

    result = RunAtomicLatencyBenchmark(num_iterations, num_warmups,
                                       atomic_loop_size);
    results.emplace_back("atomic", result);

    result = RunConditionVariableLatencyBenchmark(num_iterations, num_warmups,
                                                  cv_loop_size);
    results.emplace_back("condition_variable", result);

    result = RunSemaphoreLatencyBenchmark(num_iterations, num_warmups,
                                          semaphore_loop_size);
    results.emplace_back("semaphore", result);

    result = RunStatfsLatencyBenchmark(num_iterations, num_warmups,
                                       statfs_loop_size);
    results.emplace_back("statfs", result);

    result = RunFstatfsLatencyBenchmark(num_iterations, num_warmups,
                                        fstatfs_loop_size);
    results.emplace_back("fstatfs", result);

    result = RunGetpidLatencyBenchmark(num_iterations, num_warmups,
                                       getpid_loop_size);
    results.emplace_back("getpid", result);

    // Output all results at the end
    for (const auto &benchmark_result : results) {
      std::cout << benchmark_result.first << ": "
                << benchmark_result.second * 1e9 << " ns" << std::endl;
    }

    return 0;
  } else if (type == "atomic") {
    result = RunAtomicLatencyBenchmark(num_iterations, num_warmups,
                                       atomic_loop_size);
    std::cout << "Atomic benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "condition_variable") {
    result = RunConditionVariableLatencyBenchmark(num_iterations, num_warmups,
                                                  cv_loop_size);
    std::cout << "Condition Variable benchmark result: " << result * 1e9
              << " ns\n";
  } else if (type == "semaphore") {
    result = RunSemaphoreLatencyBenchmark(num_iterations, num_warmups,
                                          semaphore_loop_size);
    std::cout << "Semaphore benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "statfs") {
    result = RunStatfsLatencyBenchmark(num_iterations, num_warmups,
                                       statfs_loop_size);
    std::cout << "Statfs benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "fstatfs") {
    result = RunFstatfsLatencyBenchmark(num_iterations, num_warmups,
                                        fstatfs_loop_size);
    std::cout << "Fstatfs benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "getpid") {
    result = RunGetpidLatencyBenchmark(num_iterations, num_warmups,
                                       getpid_loop_size);
    std::cout << "Getpid benchmark result: " << result * 1e9 << " ns\n";
  } else {
    LOG(ERROR)
        << "Unknown benchmark type: " << type
        << ". Available types: atomic, condition_variable, semaphore, statfs, "
           "fstatfs, getpid, all";
    return 1;
  }

  return 0;
}
