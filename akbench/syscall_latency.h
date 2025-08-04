#pragma once

#include <cstdint>

#include "common.h"

BenchmarkResult RunStatfsLatencyBenchmark(int num_iterations, int num_warmups,
                                          uint64_t loop_size);
BenchmarkResult RunFstatfsLatencyBenchmark(int num_iterations, int num_warmups,
                                           uint64_t loop_size);
BenchmarkResult RunGetpidLatencyBenchmark(int num_iterations, int num_warmups,
                                          uint64_t loop_size);
