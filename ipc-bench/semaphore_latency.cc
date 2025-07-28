#include "semaphore_latency.h"

#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

#include "common.h"

namespace {

const std::string SEM_NAME_PARENT = "/sem_latency_parent";
const std::string SEM_NAME_CHILD = "/sem_latency_child";

void CleanupSemaphores() {
  sem_unlink(SEM_NAME_PARENT.c_str());
  sem_unlink(SEM_NAME_CHILD.c_str());
}

void ParentProcess(int num_iterations, int num_warmups, uint64_t loop_size,
                   std::vector<double> &durations) {
  // Open existing semaphores
  sem_t *parent_sem = sem_open(SEM_NAME_PARENT.c_str(), 0);
  CHECK(parent_sem != SEM_FAILED)
      << "Failed to open parent semaphore: " << strerror(errno);

  sem_t *child_sem = sem_open(SEM_NAME_CHILD.c_str(), 0);
  CHECK(child_sem != SEM_FAILED)
      << "Failed to open child semaphore: " << strerror(errno);

  for (int i = 0; i < num_iterations + num_warmups; ++i) {
    VLOG(1) << "Parent: Starting iteration " << i + 1 << "/"
            << (num_iterations + num_warmups);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (uint64_t j = 0; j < loop_size; ++j) {
      // Signal child
      sem_post(child_sem);
      // Wait for child to signal back
      sem_wait(parent_sem);
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end_time - start_time;
      durations.push_back(duration.count() / 2 / loop_size);
      VLOG(1) << "Parent: Iteration " << i + 1 << " takes " << duration.count()
              << " seconds.";
    }
  }

  sem_close(parent_sem);
  sem_close(child_sem);
}

void ChildProcess(uint64_t loop_size, int total_iterations) {
  // Open existing semaphores
  sem_t *parent_sem = sem_open(SEM_NAME_PARENT.c_str(), 0);
  CHECK(parent_sem != SEM_FAILED)
      << "Failed to open parent semaphore: " << strerror(errno);

  sem_t *child_sem = sem_open(SEM_NAME_CHILD.c_str(), 0);
  CHECK(child_sem != SEM_FAILED)
      << "Failed to open child semaphore: " << strerror(errno);

  for (int i = 0; i < total_iterations; ++i) {
    VLOG(1) << "Child: Starting iteration " << i + 1 << "/" << total_iterations;

    for (uint64_t j = 0; j < loop_size; ++j) {
      // Wait for parent signal
      sem_wait(child_sem);
      // Signal parent back
      sem_post(parent_sem);
    }
  }

  sem_close(parent_sem);
  sem_close(child_sem);
}

} // namespace

double RunSemaphoreBenchmark(int num_iterations, int num_warmups,
                             uint64_t loop_size) {
  CleanupSemaphores();

  // Create semaphores before forking
  sem_t *parent_sem = sem_open(SEM_NAME_PARENT.c_str(), O_CREAT, 0644, 0);
  CHECK(parent_sem != SEM_FAILED)
      << "Failed to create parent semaphore: " << strerror(errno);

  sem_t *child_sem = sem_open(SEM_NAME_CHILD.c_str(), O_CREAT, 0644, 0);
  CHECK(child_sem != SEM_FAILED)
      << "Failed to create child semaphore: " << strerror(errno);

  // Close semaphores in main process (they will be opened in child processes)
  sem_close(parent_sem);
  sem_close(child_sem);

  std::vector<double> durations;

  pid_t pid = fork();

  if (pid == -1) {
    LOG(FATAL) << "Fork failed: " << strerror(errno);
  }

  if (pid == 0) {
    // Child process
    ChildProcess(loop_size, num_iterations + num_warmups);
    exit(0);
  } else {
    // Parent process
    ParentProcess(num_iterations, num_warmups, loop_size, durations);

    // Wait for child process to finish
    waitpid(pid, nullptr, 0);

    CleanupSemaphores();

    return CalculateOneTripDuration(durations);
  }
}
