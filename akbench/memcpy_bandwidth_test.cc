#include "memcpy_bandwidth.h"

#include <cstdint>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

#include "aklog.h"

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);

  constexpr int num_iterations = 3;
  constexpr int num_warmups = 0;
  constexpr uint64_t data_size = 1024;

  const double bandwidth = RunMemcpyBandwidthBenchmark(num_iterations, num_warmups, data_size);
  
  AKCHECK(bandwidth >= 0.0, "Bandwidth should be non-negative");
  AKLOG(aklog::LogLevel::INFO, "memcpy_bandwidth test passed");
  
  return 0;
}