#pragma once

#include <cstdint>

double RunConditionVariableLatencyBenchmark(int num_iterations, int num_warmups,
                                            uint64_t loop_size);
