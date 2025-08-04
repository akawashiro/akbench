#include "mmap_bandwidth.h"

#include <cstdint>

#include "aklog.h"

int main(int argc, char *argv[]) {

  constexpr int num_iterations = 3;
  constexpr int num_warmups = 0;
  constexpr uint64_t data_size = 1024;
  constexpr uint64_t buffer_size = 1024;

  const BenchmarkResult result = RunMmapBandwidthBenchmark(
      num_iterations, num_warmups, data_size, buffer_size);

  AKCHECK(result.average >= 0.0, "Bandwidth should be non-negative");
  AKLOG(aklog::LogLevel::INFO, "mmap_bandwidth test passed");

  return 0;
}