#include "barrier_latency.h"

#include <chrono>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

#include "barrier.h"
#include "common.h"

namespace {

const std::string BARRIER_ID = "/BarrierLatencyTest";
const int NUM_PROCESSES = 2;

void ChildBarrierProcess(uint64_t loop_size) {
  SenseReversingBarrier barrier(NUM_PROCESSES, BARRIER_ID);

  for (uint64_t i = 0; i < loop_size; ++i) {
    barrier.Wait();
  }
}

} // namespace

double RunBarrierLatencyBenchmark(int num_iterations, int num_warmups,
                                  uint64_t loop_size) {
  // Clear any existing barrier resources
  SenseReversingBarrier::ClearResource(BARRIER_ID);

  VLOG(1) << "Running barrier latency benchmark with " << NUM_PROCESSES
          << " processes, " << loop_size << " iterations";

  auto run_single_benchmark = [&]() -> double {
    std::vector<int> pids;

    // Fork child processes (only 1 child process for 2-process barrier)
    for (int i = 0; i < NUM_PROCESSES - 1; ++i) {
      int pid = fork();
      CHECK(pid >= 0) << "Fork failed: " << strerror(errno);

      if (pid == 0) {
        // Child process
        ChildBarrierProcess(loop_size);
        exit(0);
      } else {
        pids.push_back(pid);
      }
    }

    // Parent process runs the benchmark
    SenseReversingBarrier barrier(NUM_PROCESSES, BARRIER_ID);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < loop_size; ++i) {
      barrier.Wait();
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    // Wait for all child processes to finish
    for (int child_pid : pids) {
      waitpid(child_pid, nullptr, 0);
    }

    double duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             end_time - start_time)
                             .count();

    // Return latency per barrier operation
    return duration_ns / static_cast<double>(loop_size);
  };

  // Run warmup iterations
  for (int i = 0; i < num_warmups; ++i) {
    VLOG(2) << "Warmup iteration " << (i + 1) << "/" << num_warmups;
    run_single_benchmark();
    SenseReversingBarrier::ClearResource(BARRIER_ID);
  }

  // Run actual measurement iterations
  std::vector<double> measurements;
  for (int i = 0; i < num_iterations; ++i) {
    VLOG(2) << "Measurement iteration " << (i + 1) << "/" << num_iterations;
    double latency_ns = run_single_benchmark();
    measurements.push_back(latency_ns);
    SenseReversingBarrier::ClearResource(BARRIER_ID);
  }

  // Calculate and return median latency in seconds
  double median_latency_ns = CalculateOneTripDuration(measurements);
  VLOG(1) << "Barrier latency (median): " << median_latency_ns << " ns";

  return median_latency_ns / 1e9; // Convert to seconds
}
