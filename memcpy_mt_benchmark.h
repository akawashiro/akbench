#pragma once

#include <cstdint>

int RunMemcpyMtBenchmark(int num_iterations, int num_warmups,
                         uint64_t data_size);
