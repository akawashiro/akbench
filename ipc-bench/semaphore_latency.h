#pragma once

#include <cstdint>

double RunSemaphoreBenchmark(int num_iterations, int num_warmups,
                             uint64_t loop_size);
