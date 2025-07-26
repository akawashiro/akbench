#include "syscall_latency.h"

#include <chrono>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>

#include "absl/log/log.h"

#include "common.h"

double RunStatfsBenchmark(int num_iterations, int num_warmups,
                          uint64_t loop_size) {
  VLOG(1) << "Running statfs benchmark with " << num_iterations
          << " iterations, " << num_warmups << " warmups, and " << loop_size
          << " operations per iteration";

  // Use current directory for statfs
  const char *path = ".";

  // Warmup
  for (int w = 0; w < num_warmups; ++w) {
    struct statfs buf;
    for (uint64_t i = 0; i < loop_size; ++i) {
      statfs(path, &buf);
    }
  }

  // Actual measurements
  std::vector<double> durations;
  durations.reserve(num_iterations);

  for (int iter = 0; iter < num_iterations; ++iter) {
    auto start = std::chrono::high_resolution_clock::now();

    struct statfs buf;
    for (uint64_t i = 0; i < loop_size; ++i) {
      statfs(path, &buf);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    durations.push_back(duration.count() / static_cast<double>(loop_size));
  }

  return CalculateOneTripDuration(durations);
}

double RunFstatfsBenchmark(int num_iterations, int num_warmups,
                           uint64_t loop_size) {
  VLOG(1) << "Running fstatfs benchmark with " << num_iterations
          << " iterations, " << num_warmups << " warmups, and " << loop_size
          << " operations per iteration";

  // Open current directory for fstatfs
  int fd = open(".", O_RDONLY);
  if (fd == -1) {
    LOG(ERROR) << "Failed to open current directory";
    return -1.0;
  }

  // Warmup
  for (int w = 0; w < num_warmups; ++w) {
    struct statfs buf;
    for (uint64_t i = 0; i < loop_size; ++i) {
      fstatfs(fd, &buf);
    }
  }

  std::vector<double> durations;
  durations.reserve(num_iterations);

  for (int iter = 0; iter < num_iterations; ++iter) {
    auto start_time = std::chrono::high_resolution_clock::now();

    struct statfs buf;
    for (uint64_t i = 0; i < loop_size; ++i) {
      fstatfs(fd, &buf);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    durations.push_back(duration.count() / static_cast<double>(loop_size));
  }

  close(fd);
  return CalculateOneTripDuration(durations);
}

double RunGetpidBenchmark(int num_iterations, int num_warmups,
                          uint64_t loop_size) {
  VLOG(1) << "Running getpid benchmark with " << num_iterations
          << " iterations, " << num_warmups << " warmups, and " << loop_size
          << " operations per iteration";

  for (int w = 0; w < num_warmups; ++w) {
    for (uint64_t i = 0; i < loop_size; ++i) {
      getpid();
    }
  }

  std::vector<double> durations;

  for (int iter = 0; iter < num_iterations; ++iter) {
    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < loop_size; ++i) {
      getpid();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    durations.push_back(duration.count() / static_cast<double>(loop_size));
  }

  return CalculateOneTripDuration(durations);
}
