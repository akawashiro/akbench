#include "atomic_latency.h"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

#include "common.h"

namespace {

void ParentFlip(std::atomic<bool> *parent, const std::atomic<bool> &child,
                const uint64_t loop_size) {
  for (uint64_t i = 0; i < loop_size; ++i) {
    parent->store(true);
    while (!child.load()) {
      ;
    }
    parent->store(false);
    while (child.load()) {
      ;
    }
  }
}

void ChildFlip(std::atomic<bool> *child, const std::atomic<bool> &parent,
               const uint64_t loop_size) {
  for (uint64_t i = 0; i < loop_size; ++i) {
    while (!parent.load()) {
      ;
    }
    child->store(true);
    while (parent.load()) {
      ;
    }
    child->store(false);
  }
}

} // namespace

double RunAtomicBenchmark(int num_iterations, int num_warmups,
                          uint64_t loop_size) {
  std::atomic<bool> parent{false}, child{false};

  std::vector<double> durations;
  for (int i = 0; i < num_iterations + num_warmups; i++) {
    VLOG(1) << "Starting iteration " << i + 1 << "/"
            << (num_iterations + num_warmups);
    std::thread child_thread([&child, &parent, loop_size]() {
      ChildFlip(&child, parent, loop_size);
    });

    auto start_time = std::chrono::high_resolution_clock::now();
    ParentFlip(&parent, child, loop_size);
    auto end_time = std::chrono::high_resolution_clock::now();

    child_thread.join();
    VLOG(1) << "Iteration " << i + 1 << " takes "
            << std::chrono::duration<double>(end_time - start_time).count()
            << " seconds.";

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end_time - start_time;
      durations.push_back(duration.count() / 4 / loop_size);
    }
  }

  return CalculateOneTripDuration(durations);
}
