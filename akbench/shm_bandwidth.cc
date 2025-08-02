#include "shm_bandwidth.h"

#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#include "aklog.h"

#include "barrier.h"
#include "common.h"

namespace {
const std::string SHM_NAME = GenerateUniqueName("/shm_bandwidth_test");
const std::string BARRIER_ID = GenerateUniqueName("/shm_benchmark");

struct SharedBuffer {
  size_t data_size[2];
  char data[];
};

void CleanupResources() { shm_unlink(SHM_NAME.c_str()); }

double ReceiveProcess(int num_warmups, int num_iterations, uint64_t data_size,
                      uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);
  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    bool is_warmup = iteration < num_warmups;

    if (is_warmup) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Warm-up {}/{}", ReceivePrefix(iteration), iteration,
                        num_warmups));
    } else {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Starting iteration...", ReceivePrefix(iteration)));
    }

    // Create shared memory
    const size_t shared_buffer_size = sizeof(SharedBuffer) + 2 * buffer_size;
    int shm_fd = shm_open(SHM_NAME.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: shm_open: {}", strerror(errno)));
    }

    if (ftruncate(shm_fd, shared_buffer_size) == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: ftruncate: {}", strerror(errno)));
    }

    SharedBuffer *shared_buffer = static_cast<SharedBuffer *>(
        mmap(NULL, shared_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED,
             shm_fd, 0));
    memset(shared_buffer, 0, shared_buffer_size);
    if (shared_buffer == MAP_FAILED) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: mmap: {}", strerror(errno)));
    }

    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Shared memory and semaphores initialized",
                      ReceivePrefix(iteration)));
    barrier.Wait();

    std::vector<uint8_t> received_data(data_size, 0);

    barrier.Wait();
    uint64_t bytes_received = 0;
    constexpr uint64_t PIPELINE_INDEX = 1;
    const uint64_t n_pipeline = (data_size + buffer_size - 1) / buffer_size + 1;
    auto start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < n_pipeline; ++i) {
      barrier.Wait();
      char *buffer_ptr =
          shared_buffer->data + ((i + PIPELINE_INDEX) % 2) * buffer_size;
      memcpy(received_data.data() + bytes_received, buffer_ptr,
             shared_buffer->data_size[(i + PIPELINE_INDEX) % 2]);
      bytes_received += shared_buffer->data_size[(i + PIPELINE_INDEX) % 2];
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());

      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Time taken: {} ms.", ReceivePrefix(iteration),
                        elapsed_time.count() * 1000));
    }

    // Verify received data (always, even during warmup)
    if (!VerifyDataReceived(received_data, data_size)) {
      AKLOG(aklog::LogLevel::FATAL, std::format("{}Data verification failed!",
                                                ReceivePrefix(iteration)));
    } else {
      AKLOG(aklog::LogLevel::DEBUG, std::format("{}Data verification passed.",
                                                ReceivePrefix(iteration)));
    }

    munmap(shared_buffer, shared_buffer_size);
    close(shm_fd);
    CleanupResources();
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  AKLOG(aklog::LogLevel::INFO,
        std::format("Receive bandwidth: {}{}.", bandwidth / (1 << 30),
                    GIBYTE_PER_SEC_UNIT));

  return bandwidth;
}

void SendProcess(int num_warmups, int num_iterations, uint64_t data_size,
                 uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);
  std::vector<uint8_t> data_to_send = GenerateDataToSend(data_size);
  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    bool is_warmup = iteration < num_warmups;

    if (is_warmup) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Warm-up {}/{}", SendPrefix(iteration), iteration,
                        num_warmups));
    } else {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Starting iteration...", SendPrefix(iteration)));
    }

    barrier.Wait();
    // Open existing shared memory
    const size_t shared_buffer_size = sizeof(SharedBuffer) + 2 * buffer_size;
    int shm_fd = shm_open(SHM_NAME.c_str(), O_RDWR, 0666);
    if (shm_fd == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("send: shm_open: {}", strerror(errno)));
    }

    SharedBuffer *shared_buffer = static_cast<SharedBuffer *>(
        mmap(NULL, shared_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED,
             shm_fd, 0));
    if (shared_buffer == MAP_FAILED) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("send: mmap: {}", strerror(errno)));
    }

    barrier.Wait();
    uint64_t bytes_send = 0;
    constexpr uint64_t PIPELINE_INDEX = 0;
    const uint64_t n_pipeline = (data_size + buffer_size - 1) / buffer_size + 1;
    auto start_time = std::chrono::high_resolution_clock::now();
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}n_pipeline: {}", SendPrefix(iteration), n_pipeline));
    for (uint64_t i = 0; i < n_pipeline; ++i) {
      barrier.Wait();
      const size_t size_to_send = std::min(data_size - bytes_send, buffer_size);
      char *buffer_ptr =
          shared_buffer->data + ((i + PIPELINE_INDEX) % 2) * buffer_size;
      memcpy(buffer_ptr, data_to_send.data() + bytes_send, size_to_send);
      shared_buffer->data_size[(i + PIPELINE_INDEX) % 2] = size_to_send;
      bytes_send += size_to_send;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Time taken: {} ms.", SendPrefix(iteration),
                        elapsed_time.count() * 1000));
    }

    munmap(shared_buffer, shared_buffer_size);
    close(shm_fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  AKLOG(aklog::LogLevel::INFO,
        std::format("Send bandwidth: {}{}.", bandwidth / (1 << 30),
                    GIBYTE_PER_SEC_UNIT));
}

} // namespace

double RunShmBandwidthBenchmark(int num_iterations, int num_warmups,
                                uint64_t data_size, uint64_t buffer_size) {
  SenseReversingBarrier::ClearResource(BARRIER_ID);
  CleanupResources();

  pid_t pid = fork();

  if (pid == -1) {
    AKLOG(aklog::LogLevel::FATAL,
          std::format("Fork failed: {}", strerror(errno)));
  }

  if (pid == 0) {
    SendProcess(num_warmups, num_iterations, data_size, buffer_size);
    exit(0);
  } else {
    double bandwidth =
        ReceiveProcess(num_warmups, num_iterations, data_size, buffer_size);
    waitpid(pid, nullptr, 0);
    return bandwidth;
  }
}
