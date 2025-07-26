#include <iostream>
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

ABSL_FLAG(std::string, type, "",
          "Benchmark type to run (atomic, condition_variable, all)");
ABSL_FLAG(int, num_iterations, 10,
          "Number of measurement iterations (minimum 3)");
ABSL_FLAG(int, num_warmups, 3, "Number of warmup iterations");
ABSL_FLAG(uint64_t, loop_size, (1 << 20),
          "number of iterations in each "
          "measurement loop (default: 1 Mi)");
ABSL_FLAG(std::optional<int>, vlog, std::nullopt,
          "Show VLOG messages lower than this level.");

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage(
      "Latency benchmark tool. Use --type to specify benchmark type.");
  absl::ParseCommandLine(argc, argv);

  const std::string type = absl::GetFlag(FLAGS_type);
  const int num_iterations = absl::GetFlag(FLAGS_num_iterations);
  const int num_warmups = absl::GetFlag(FLAGS_num_warmups);
  const uint64_t loop_size = absl::GetFlag(FLAGS_loop_size);

  if (type.empty()) {
    LOG(ERROR) << "Must specify --type. Available types: atomic, "
                  "condition_variable, all";
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
    // Collect all benchmark results first, then output at the end
    std::vector<std::pair<std::string, double>> results;

    result = RunAtomicBenchmark(num_iterations, num_warmups, loop_size);
    results.emplace_back("atomic", result);

    result =
        RunConditionVariableBenchmark(num_iterations, num_warmups, loop_size);
    results.emplace_back("condition_variable", result);

    // Output all results at the end
    for (const auto &benchmark_result : results) {
      std::cout << benchmark_result.first << ": "
                << benchmark_result.second * 1e9 << " ns" << std::endl;
    }

    return 0;
  } else if (type == "atomic") {
    result = RunAtomicBenchmark(num_iterations, num_warmups, loop_size);
    std::cout << "Atomic benchmark result: " << result * 1e9 << " ns\n";
  } else if (type == "condition_variable") {
    result =
        RunConditionVariableBenchmark(num_iterations, num_warmups, loop_size);
    std::cout << "Condition Variable benchmark result: " << result * 1e9
              << " ns\n";
  } else {
    LOG(ERROR) << "Unknown benchmark type: " << type
               << ". Available types: atomic, condition_variable, all";
    return 1;
  }

  return 0;
}
