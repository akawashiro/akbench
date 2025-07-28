#pragma once

#include <cstdint>

double RunMemcpyBandwidthBenchmark(int num_iterations, int num_warmups,
                                   uint64_t data_size);
