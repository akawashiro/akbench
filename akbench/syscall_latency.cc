#include "syscall_latency.h"

#include <chrono>
#include <fcntl.h>
#include <format>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "aklog.h"

#include "common.h"

BenchmarkResult RunStatfsLatencyBenchmark(int num_iterations, int num_warmups,
                                          uint64_t loop_size) {
  AKLOG(aklog::LogLevel::DEBUG,
        std::format("Running statfs benchmark with {} iterations, {} warmups, "
                    "and {} operations per iteration",
                    num_iterations, num_warmups, loop_size));

  const char *path = ".";

  std::vector<double> durations;
  for (int i = 0; i < num_iterations + num_warmups; i++) {
    auto start = std::chrono::high_resolution_clock::now();

    struct statfs buf;
    for (uint64_t j = 0; j < loop_size; ++j) {
      statfs(path, &buf);
    }

    auto end = std::chrono::high_resolution_clock::now();

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end - start;
      durations.push_back(duration.count() / static_cast<double>(loop_size));
    }
  }

  return CalculateOneTripDuration(durations);
}

BenchmarkResult RunFstatfsLatencyBenchmark(int num_iterations, int num_warmups,
                                           uint64_t loop_size) {
  AKLOG(aklog::LogLevel::DEBUG,
        std::format("Running fstatfs benchmark with {} iterations, {} warmups, "
                    "and {} operations per iteration",
                    num_iterations, num_warmups, loop_size));

  int fd = open(".", O_RDONLY);
  if (fd == -1) {
    AKLOG(aklog::LogLevel::ERROR, "Failed to open current directory");
    return {-1.0, 0.0};
  }

  std::vector<double> durations;
  for (int i = 0; i < num_iterations + num_warmups; i++) {
    auto start_time = std::chrono::high_resolution_clock::now();

    struct statfs buf;
    for (uint64_t j = 0; j < loop_size; ++j) {
      fstatfs(fd, &buf);
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end_time - start_time;
      durations.push_back(duration.count() / static_cast<double>(loop_size));
    }
  }

  close(fd);
  return CalculateOneTripDuration(durations);
}

BenchmarkResult RunGetpidLatencyBenchmark(int num_iterations, int num_warmups,
                                          uint64_t loop_size) {
  AKLOG(aklog::LogLevel::DEBUG,
        std::format("Running getpid benchmark with {} iterations, {} warmups, "
                    "and {} operations per iteration",
                    num_iterations, num_warmups, loop_size));

  std::vector<double> durations;
  for (int i = 0; i < num_iterations + num_warmups; i++) {
    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t j = 0; j < loop_size; ++j) {
      getpid();
    }

    auto end = std::chrono::high_resolution_clock::now();

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end - start;
      durations.push_back(duration.count() / static_cast<double>(loop_size));
    }
  }

  return CalculateOneTripDuration(durations);
}
