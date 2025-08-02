#include "semaphore_latency.h"

#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#include "aklog.h"

#include "common.h"

namespace {

const std::string SEM_NAME_PARENT = GenerateUniqueName("/sem_latency_parent");
const std::string SEM_NAME_CHILD = GenerateUniqueName("/sem_latency_child");

void CleanupSemaphores() {
  sem_unlink(SEM_NAME_PARENT.c_str());
  sem_unlink(SEM_NAME_CHILD.c_str());
}

std::vector<double> ParentProcess(int num_iterations, int num_warmups,
                                  uint64_t loop_size) {
  std::vector<double> durations;

  sem_t *parent_sem = sem_open(SEM_NAME_PARENT.c_str(), 0);
  AKCHECK(parent_sem != SEM_FAILED,
          std::format("Failed to open parent semaphore: {}", strerror(errno)));

  sem_t *child_sem = sem_open(SEM_NAME_CHILD.c_str(), 0);
  AKCHECK(child_sem != SEM_FAILED,
          std::format("Failed to open child semaphore: {}", strerror(errno)));

  for (int i = 0; i < num_iterations + num_warmups; ++i) {
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("Parent: Starting iteration {}/{}", i + 1,
                      (num_iterations + num_warmups)));

    auto start_time = std::chrono::high_resolution_clock::now();

    for (uint64_t j = 0; j < loop_size; ++j) {
      sem_post(child_sem);
      sem_wait(parent_sem);
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    if (i >= num_warmups) {
      std::chrono::duration<double> duration = end_time - start_time;
      durations.push_back(duration.count() / 2 / loop_size);
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("Parent: Iteration {} takes {} seconds.", i + 1,
                        duration.count()));
    }
  }

  sem_close(parent_sem);
  sem_close(child_sem);

  return durations;
}

void ChildProcess(uint64_t loop_size, int total_iterations) {
  sem_t *parent_sem = sem_open(SEM_NAME_PARENT.c_str(), 0);
  AKCHECK(parent_sem != SEM_FAILED,
          std::format("Failed to open parent semaphore: {}", strerror(errno)));

  sem_t *child_sem = sem_open(SEM_NAME_CHILD.c_str(), 0);
  AKCHECK(child_sem != SEM_FAILED,
          std::format("Failed to open child semaphore: {}", strerror(errno)));

  for (int i = 0; i < total_iterations; ++i) {
    AKLOG(aklog::LogLevel::DEBUG, std::format("Child: Starting iteration {}/{}",
                                              i + 1, total_iterations));

    for (uint64_t j = 0; j < loop_size; ++j) {
      sem_wait(child_sem);
      sem_post(parent_sem);
    }
  }

  sem_close(parent_sem);
  sem_close(child_sem);
}

} // namespace

double RunSemaphoreLatencyBenchmark(int num_iterations, int num_warmups,
                                    uint64_t loop_size) {
  CleanupSemaphores();

  // Create semaphores before forking
  sem_t *parent_sem = sem_open(SEM_NAME_PARENT.c_str(), O_CREAT, 0644, 0);
  AKCHECK(
      parent_sem != SEM_FAILED,
      std::format("Failed to create parent semaphore: {}", strerror(errno)));

  sem_t *child_sem = sem_open(SEM_NAME_CHILD.c_str(), O_CREAT, 0644, 0);
  AKCHECK(child_sem != SEM_FAILED,
          std::format("Failed to create child semaphore: {}", strerror(errno)));

  sem_close(parent_sem);
  sem_close(child_sem);

  pid_t pid = fork();

  if (pid == -1) {
    AKLOG(aklog::LogLevel::FATAL,
          std::format("Fork failed: {}", strerror(errno)));
  }

  if (pid == 0) {
    ChildProcess(loop_size, num_iterations + num_warmups);
    exit(0);
  } else {
    std::vector<double> durations =
        ParentProcess(num_iterations, num_warmups, loop_size);

    waitpid(pid, nullptr, 0);
    CleanupSemaphores();
    return CalculateOneTripDuration(durations);
  }
}
