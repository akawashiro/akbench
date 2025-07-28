#pragma once

#include <cstdint>

double RunStatfsLatencyBenchmark(int num_iterations, int num_warmups,
                                 uint64_t loop_size);
double RunFstatfsLatencyBenchmark(int num_iterations, int num_warmups,
                                  uint64_t loop_size);
double RunGetpidLatencyBenchmark(int num_iterations, int num_warmups,
                                 uint64_t loop_size);
