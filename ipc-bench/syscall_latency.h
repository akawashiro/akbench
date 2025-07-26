#pragma once

#include <cstdint>

double RunStatfsBenchmark(int num_iterations, int num_warmups,
                          uint64_t loop_size);
double RunFstatfsBenchmark(int num_iterations, int num_warmups,
                           uint64_t loop_size);
double RunGetpidBenchmark(int num_iterations, int num_warmups,
                          uint64_t loop_size);
