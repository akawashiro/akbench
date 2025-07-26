#include "condition_variable_latency.h"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace {

void ParentFlip(std::condition_variable *parent_cv,
                std::condition_variable *child_cv, std::mutex *parent_mutex,
                std::mutex *child_mutex, bool *parent_ready, bool *child_ready,
                const uint64_t loop_size) {
  for (uint64_t i = 0; i < loop_size; ++i) {
    // Signal child to proceed
    {
      std::lock_guard<std::mutex> lock(*parent_mutex);
      *parent_ready = true;
    }
    parent_cv->notify_one();

    // Wait for child to signal back
    {
      std::unique_lock<std::mutex> lock(*child_mutex);
      child_cv->wait(lock, [child_ready] { return *child_ready; });
      *child_ready = false;
    }

    // Reset parent ready for next iteration
    {
      std::lock_guard<std::mutex> lock(*parent_mutex);
      *parent_ready = false;
    }
  }
}

void ChildFlip(std::condition_variable *parent_cv,
               std::condition_variable *child_cv, std::mutex *parent_mutex,
               std::mutex *child_mutex, bool *parent_ready, bool *child_ready,
               const uint64_t loop_size) {
  for (uint64_t i = 0; i < loop_size; ++i) {
    // Wait for parent to signal
    {
      std::unique_lock<std::mutex> lock(*parent_mutex);
      parent_cv->wait(lock, [parent_ready] { return *parent_ready; });
    }

    // Signal back to parent
    {
      std::lock_guard<std::mutex> lock(*child_mutex);
      *child_ready = true;
    }
    child_cv->notify_one();
  }
}

double CalculateOneTripDuration(const std::vector<double> &durations,
                                const uint64_t loop_size) {
  CHECK(durations.size() >= 3);
  std::vector<double> sorted_durations = durations;
  std::sort(sorted_durations.begin(), sorted_durations.end());
  double average_duration = std::accumulate(sorted_durations.begin() + 1,
                                            sorted_durations.end() - 1, 0.0) /
                            (sorted_durations.size() - 2);
  return average_duration / (4 * loop_size);
}

} // namespace

double RunConditionVariableBenchmark(int num_iterations, int num_warmups,
                                     uint64_t loop_size) {
  std::condition_variable parent_cv, child_cv;
  std::mutex parent_mutex, child_mutex;
  bool parent_ready = false, child_ready = false;

  std::vector<double> durations;
  for (int i = 0; i < num_iterations + num_warmups; i++) {
    VLOG(1) << "Starting iteration " << i + 1 << "/"
            << (num_iterations + num_warmups);
    std::thread child_thread([&]() {
      ChildFlip(&parent_cv, &child_cv, &parent_mutex, &child_mutex,
                &parent_ready, &child_ready, loop_size);
    });

    auto start_time = std::chrono::high_resolution_clock::now();
    ParentFlip(&parent_cv, &child_cv, &parent_mutex, &child_mutex,
               &parent_ready, &child_ready, loop_size);
    auto end_time = std::chrono::high_resolution_clock::now();

    child_thread.join();
    VLOG(1) << "Iteration " << i + 1 << " takes "
            << std::chrono::duration<double>(end_time - start_time).count()
            << " seconds.";

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end_time - start_time;
      durations.push_back(duration.count());
    }

    // Reset state for next iteration
    parent_ready = false;
    child_ready = false;
  }

  return CalculateOneTripDuration(durations, loop_size);
}
