#include "mmap_benchmark.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "absl/log/log.h"

#include "barrier.h"
#include "common.h"

namespace {
const std::string MMAP_FILE_PATH = "/tmp/mmap_bandwidth_test.dat";
const std::string BARRIER_ID = "/mmap_benchmark";

struct MmapBuffer {
  size_t data_size[2];
  char data[];
};

void SendProcess(const int num_warmups, const int num_iterations,
                 const uint64_t data_size, const uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);
  barrier.Wait();

  std::vector<uint8_t> data_to_send = GenerateDataToSend(data_size);
  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    int fd = open(MMAP_FILE_PATH.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd == -1) {
      LOG(FATAL) << "send: open: " << strerror(errno);
      return;
    }

    size_t total_size = sizeof(MmapBuffer) + 2 * buffer_size;
    if (ftruncate(fd, total_size) == -1) {
      LOG(FATAL) << "send: ftruncate: " << strerror(errno);
      close(fd);
      return;
    }

    // Map the file into memory
    void *mapped_region =
        mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_region == MAP_FAILED) {
      LOG(FATAL) << "send: mmap: " << strerror(errno);
    }
    barrier.Wait();

    MmapBuffer *mmap_buffer = static_cast<MmapBuffer *>(mapped_region);
    memset(mmap_buffer, 0, total_size);

    bool is_warmup = iteration < num_warmups;

    if (is_warmup) {
      VLOG(1) << "Sender: Warm-up " << iteration << "/" << num_warmups;
    } else {
      VLOG(1) << SendPrefix(iteration) << "Starting iteration...";
    }

    barrier.Wait();
    uint64_t bytes_sent = 0;
    constexpr uint64_t PIPELINE_INDEX = 0;
    const uint64_t n_pipeline = (data_size + buffer_size - 1) / buffer_size + 1;
    auto start_time = std::chrono::high_resolution_clock::now();
    VLOG(1) << SendPrefix(iteration) << "n_pipeline: " << n_pipeline;
    for (uint64_t i = 0; i < n_pipeline; ++i) {
      barrier.Wait();
      const size_t size_to_send = std::min(data_size - bytes_sent, buffer_size);
      char *buffer_ptr =
          mmap_buffer->data + ((i + PIPELINE_INDEX) % 2) * buffer_size;
      memcpy(buffer_ptr, data_to_send.data() + bytes_sent, size_to_send);
      mmap_buffer->data_size[(i + PIPELINE_INDEX) % 2] = size_to_send;
      bytes_sent += size_to_send;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      VLOG(1) << "Sender: Time taken: " << elapsed_time.count() * 1000
              << " ms.";
    }

    munmap(mapped_region, total_size);
    close(fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  LOG(INFO) << "Send bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";

  VLOG(1) << "Sender: Exiting.";
}

double ReceiveProcess(const int num_warmups, const int num_iterations,
                      const uint64_t data_size, const uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);
  barrier.Wait();
  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    barrier.Wait();
    int fd = open(MMAP_FILE_PATH.c_str(), O_RDWR);
    if (fd == -1) {
      LOG(FATAL) << "receive: open: " << strerror(errno);
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
      LOG(FATAL) << "receive: fstat: " << strerror(errno);
    }

    size_t total_size = file_stat.st_size;

    void *mapped_region =
        mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_region == MAP_FAILED) {
      LOG(FATAL) << "receive: mmap: " << strerror(errno);
    }

    MmapBuffer *mmap_buffer = static_cast<MmapBuffer *>(mapped_region);

    bool is_warmup = iteration < num_warmups;

    if (is_warmup) {
      VLOG(1) << "Receiver: Warm-up " << iteration << "/" << num_warmups;
    } else {
      VLOG(1) << ReceivePrefix(iteration) << "Starting iteration...";
    }

    std::vector<uint8_t> received_data(data_size, 0);

    barrier.Wait();
    uint64_t bytes_received = 0;
    constexpr uint64_t PIPELINE_INDEX = 1;
    const uint64_t n_pipeline = (data_size + buffer_size - 1) / buffer_size + 1;
    auto start_time = std::chrono::high_resolution_clock::now();
    for (uint64_t i = 0; i < n_pipeline; ++i) {
      barrier.Wait();
      const char *buffer_ptr =
          mmap_buffer->data + ((i + PIPELINE_INDEX) % 2) * buffer_size;
      const size_t size_to_receive =
          mmap_buffer->data_size[(i + PIPELINE_INDEX) % 2];
      memcpy(received_data.data() + bytes_received, buffer_ptr,
             size_to_receive);
      bytes_received += size_to_receive;
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());

      VLOG(1) << "Receiver: Time taken: " << elapsed_time.count() * 1000
              << " ms.";
    }

    // Verify received data (always, even during warmup)
    if (!VerifyDataReceived(received_data, data_size)) {
      LOG(FATAL) << ReceivePrefix(iteration) << "Data verification failed!";
    } else {
      VLOG(1) << ReceivePrefix(iteration) << "Data verification passed.";
    }

    munmap(mapped_region, total_size);
    close(fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  LOG(INFO) << "Receive bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";

  VLOG(1) << "Receiver: Exiting.";

  return bandwidth;
}

} // namespace

double RunMmapBenchmark(int num_iterations, int num_warmups, uint64_t data_size,
                        uint64_t buffer_size) {
  SenseReversingBarrier::ClearResource(BARRIER_ID);
  unlink(MMAP_FILE_PATH.c_str());

  pid_t pid = fork();

  if (pid == -1) {
    LOG(FATAL) << "fork: " << strerror(errno);
  }

  if (pid == 0) {
    SendProcess(num_warmups, num_iterations, data_size, buffer_size);
    exit(0);
  } else {
    double bandwidth =
        ReceiveProcess(num_warmups, num_iterations, data_size, buffer_size);
    waitpid(pid, nullptr, 0);
    unlink(MMAP_FILE_PATH.c_str());
    return bandwidth;
  }
}
