#pragma once

#include <cstdint>

double RunBarrierLatencyBenchmark(int num_iterations, int num_warmups,
                                  uint64_t loop_size);
