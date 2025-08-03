#include "condition_variable_latency.h"

#include <cstdint>

#include "aklog.h"

int main(int argc, char *argv[]) {

  constexpr int num_iterations = 3;
  constexpr int num_warmups = 0;
  constexpr uint64_t loop_size = 10;

  const double latency = RunConditionVariableLatencyBenchmark(
      num_iterations, num_warmups, loop_size);

  AKCHECK(latency >= 0.0, "Latency should be non-negative");
  AKLOG(aklog::LogLevel::INFO, "condition_variable_latency test passed");

  return 0;
}