#include "mq_bandwidth.h"

#include <cstdint>

#include "aklog.h"

int main(int argc, char *argv[]) {

  constexpr int num_iterations = 3;
  constexpr int num_warmups = 0;
  constexpr uint64_t data_size = 1024;
  constexpr uint64_t buffer_size = 1024;

  const double bandwidth = RunMqBandwidthBenchmark(num_iterations, num_warmups,
                                                   data_size, buffer_size);

  AKCHECK(bandwidth >= 0.0, "Bandwidth should be non-negative");
  AKLOG(aklog::LogLevel::INFO, "mq_bandwidth test passed");

  return 0;
}