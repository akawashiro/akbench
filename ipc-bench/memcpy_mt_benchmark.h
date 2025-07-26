#pragma once

#include <cstdint>

double RunMemcpyMtBenchmark(int num_iterations, int num_warmups,
                            uint64_t data_size, uint64_t num_threads);
