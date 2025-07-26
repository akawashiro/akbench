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

#include "atomic_benchmark.h"

ABSL_FLAG(std::string, type, "", "Benchmark type to run (atomic)");
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
      "Bandwidth benchmark tool. Use --type to specify benchmark type.");
  absl::ParseCommandLine(argc, argv);

  const std::string type = absl::GetFlag(FLAGS_type);
  const int num_iterations = absl::GetFlag(FLAGS_num_iterations);
  const int num_warmups = absl::GetFlag(FLAGS_num_warmups);
  const uint64_t loop_size = absl::GetFlag(FLAGS_loop_size);

  if (type.empty()) {
    LOG(ERROR) << "Must specify --type.";
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

  if (type == "atomic") {
    double result = RunAtomicBenchmark(num_iterations, num_warmups, loop_size);
    std::cout << "Atomic benchmark result: " << result * 1e9 << " ns\n";
  } else {
    LOG(ERROR) << "Unknown benchmark type: " << type;
    return 1;
  }

  return 0;
}
