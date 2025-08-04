#pragma once

#include <cstdint>

#include "common.h"

BenchmarkResult RunAtomicLatencyBenchmark(int num_iterations, int num_warmups,
                                          uint64_t loop_size);
