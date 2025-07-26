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

double RunGetpidBenchmark(int num_iterations, int num_warmups,
                          uint64_t loop_size) {
  VLOG(1) << "Running getpid benchmark with " << num_iterations
          << " iterations, " << num_warmups << " warmups, and " << loop_size
          << " operations per iteration";

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
