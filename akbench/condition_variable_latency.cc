#include "condition_variable_latency.h"

#include <condition_variable>
#include <cstdint>
#include <format>
#include <mutex>
#include <thread>
#include <vector>

#include "aklog.h"

#include "common.h"

namespace {

void ParentFlip(std::condition_variable *parent_cv,
                std::condition_variable *child_cv, std::mutex *parent_mutex,
                std::mutex *child_mutex, bool *parent_ready, bool *child_ready,
                const uint64_t loop_size) {
  for (uint64_t i = 0; i < loop_size; ++i) {
    {
      std::lock_guard<std::mutex> lock(*parent_mutex);
      *parent_ready = true;
    }
    parent_cv->notify_one();

    {
      std::unique_lock<std::mutex> lock(*child_mutex);
      child_cv->wait(lock, [child_ready] { return *child_ready; });
      *child_ready = false;
    }
  }
}

void ChildFlip(std::condition_variable *parent_cv,
               std::condition_variable *child_cv, std::mutex *parent_mutex,
               std::mutex *child_mutex, bool *parent_ready, bool *child_ready,
               const uint64_t loop_size) {
  for (uint64_t i = 0; i < loop_size; ++i) {
    {
      std::unique_lock<std::mutex> lock(*parent_mutex);
      parent_cv->wait(lock, [parent_ready] { return *parent_ready; });
      *parent_ready = false;
    }

    {
      std::lock_guard<std::mutex> lock(*child_mutex);
      *child_ready = true;
    }
    child_cv->notify_one();
  }
}

} // namespace

double RunConditionVariableLatencyBenchmark(int num_iterations, int num_warmups,
                                            uint64_t loop_size) {
  std::condition_variable parent_cv, child_cv;
  std::mutex parent_mutex, child_mutex;
  bool parent_ready = false, child_ready = false;

  std::vector<double> durations;
  for (int i = 0; i < num_iterations + num_warmups; i++) {
    AKLOG(aklog::LogLevel::DEBUG, std::format("Starting iteration {}/{}", i + 1,
                                              (num_iterations + num_warmups)));
    std::thread child_thread([&]() {
      ChildFlip(&parent_cv, &child_cv, &parent_mutex, &child_mutex,
                &parent_ready, &child_ready, loop_size);
    });

    auto start_time = std::chrono::high_resolution_clock::now();
    ParentFlip(&parent_cv, &child_cv, &parent_mutex, &child_mutex,
               &parent_ready, &child_ready, loop_size);
    auto end_time = std::chrono::high_resolution_clock::now();

    child_thread.join();
    AKLOG(aklog::LogLevel::DEBUG,
          std::format(
              "Iteration {} takes {} seconds.", i + 1,
              std::chrono::duration<double>(end_time - start_time).count()));

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end_time - start_time;
      durations.push_back(duration.count() / 2 / loop_size);
    }

    // Reset state for next iteration
    parent_ready = false;
    child_ready = false;
  }

  return CalculateOneTripDuration(durations);
}
