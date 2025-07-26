#pragma once

#include <cstdint>

double RunAtomicBenchmark(int num_iterations, int num_warmups,
                          uint64_t loop_size);
