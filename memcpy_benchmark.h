#pragma once

#include <cstdint>

int RunMemcpyBenchmark(int num_iterations, int num_warmups, uint64_t data_size);
