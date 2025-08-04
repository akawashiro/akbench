#include "memcpy_bandwidth.h"

#include <cstdint>

#include "aklog.h"

int main(int argc, char *argv[]) {

  constexpr int num_iterations = 3;
  constexpr int num_warmups = 0;
  constexpr uint64_t data_size = 1024;

  const BenchmarkResult result =
      RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);

  AKCHECK(result.average >= 0.0, "Bandwidth should be non-negative");
  AKLOG(aklog::LogLevel::INFO, "memcpy_bandwidth test passed");

  return 0;
}