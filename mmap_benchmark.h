#pragma once

#include <cstdint>

int RunMmapBenchmark(int num_iterations, int num_warmups, uint64_t data_size,
                     uint64_t buffer_size);
