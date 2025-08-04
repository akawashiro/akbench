#include "uds_bandwidth.h"

#include <cstdint>

#include "aklog.h"

int main(int argc, char *argv[]) {

  constexpr int num_iterations = 3;
  constexpr int num_warmups = 0;
  constexpr uint64_t data_size = 256;
  constexpr uint64_t buffer_size = 256;

  const BenchmarkResult result = RunUdsBandwidthBenchmark(
      num_iterations, num_warmups, data_size, buffer_size);

  AKCHECK(result.average >= 0.0, "Bandwidth should be non-negative");
  AKLOG(aklog::LogLevel::INFO, "uds_bandwidth test passed");

  return 0;
}