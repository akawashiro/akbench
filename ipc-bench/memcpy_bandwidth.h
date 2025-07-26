#pragma once

#include <cstdint>

double RunMemcpyBenchmark(int num_iterations, int num_warmups,
                          uint64_t data_size);
