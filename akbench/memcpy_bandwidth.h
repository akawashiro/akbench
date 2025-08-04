#pragma once

#include "common.h"
#include <cstdint>

BenchmarkResult RunMemcpyBandwidthBenchmark(int num_iterations, int num_warmups,
                                            uint64_t data_size);
