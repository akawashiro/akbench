#pragma once

#include <cstdint>

#include "common.h"

BenchmarkResult RunBarrierLatencyBenchmark(int num_iterations, int num_warmups,
                                           uint64_t loop_size);
