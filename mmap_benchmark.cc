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
const int BUFFER_SIZE = 4096;
const std::string BARRIER_ID = "/mmap_benchmark";

struct sync_data {
  std::atomic<uint64_t> bytes_written;
};

void SendProcess(int num_warmups, int num_iterations, uint64_t data_size,
                 uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

  int fd = open(MMAP_FILE_PATH.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd == -1) {
    LOG(ERROR) << "send: open: " << strerror(errno);
    return;
  }

  size_t total_size = data_size + sizeof(sync_data);
  if (ftruncate(fd, total_size) == -1) {
    LOG(ERROR) << "send: ftruncate: " << strerror(errno);
    close(fd);
    return;
  }

  // Map the file into memory
  void *mapped_region =
      mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_region == MAP_FAILED) {
    LOG(ERROR) << "send: mmap: " << strerror(errno);
    close(fd);
    return;
  }

  char *data_region = static_cast<char *>(mapped_region);
  sync_data *sync = reinterpret_cast<sync_data *>(data_region + data_size);
  sync->bytes_written = 0;

  barrier.Wait();

  std::vector<uint8_t> data_to_send = GenerateDataToSend(data_size);
  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    bool is_warmup = iteration < num_warmups;

    if (is_warmup) {
      VLOG(1) << "Sender: Warm-up " << iteration << "/" << num_warmups;
    } else {
      VLOG(1) << "Sender: Starting iteration " << iteration << "/"
              << num_iterations;
    }

    sync->bytes_written = 0;

    barrier.Wait();
    auto start_time = std::chrono::high_resolution_clock::now();
    memcpy(data_region, data_to_send.data(), data_size);
    sync->bytes_written.store(data_size);
    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      VLOG(1) << "Sender: Time taken: " << elapsed_time.count() * 1000
              << " ms.";
    }
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  double bandwidth_gibps = bandwidth / (1024.0 * 1024.0 * 1024.0);
  LOG(INFO) << "Send bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";

  munmap(mapped_region, total_size);
  close(fd);
  VLOG(1) << "Sender: Exiting.";
}

void ReceiveProcess(int num_warmups, int num_iterations, uint64_t data_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

  barrier.Wait();

  int fd = open(MMAP_FILE_PATH.c_str(), O_RDWR);
  if (fd == -1) {
    LOG(ERROR) << "receive: open: " << strerror(errno);
    return;
  }

  struct stat file_stat;
  if (fstat(fd, &file_stat) == -1) {
    LOG(ERROR) << "receive: fstat: " << strerror(errno);
    close(fd);
    return;
  }

  size_t total_size = file_stat.st_size;

  void *mapped_region =
      mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_region == MAP_FAILED) {
    LOG(ERROR) << "receive: mmap: " << strerror(errno);
    close(fd);
    return;
  }

  // Set up pointers
  char *data_region = static_cast<char *>(mapped_region);
  sync_data *sync = reinterpret_cast<sync_data *>(data_region + data_size);

  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    barrier.Wait();
    auto start_time = std::chrono::high_resolution_clock::now();
    while (sync->bytes_written.load() == 0) {
    }
    memcpy(data_region, data_region, data_size);
    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();

    bool is_warmup = iteration < num_warmups;
    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());

      VLOG(1) << "Receiver: Time taken: " << elapsed_time.count() * 1000
              << " ms.";
    }

    std::vector<uint8_t> received_data(
        reinterpret_cast<uint8_t *>(data_region),
        reinterpret_cast<uint8_t *>(data_region) + data_size);
    if (!VerifyDataReceived(received_data, data_size)) {
      LOG(ERROR) << ReceivePrefix(iteration) << "Data verification failed!";
    } else {
      VLOG(1) << ReceivePrefix(iteration) << "Data verification passed.";
    }
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  LOG(INFO) << "Receive bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";

  munmap(mapped_region, total_size);
  close(fd);
  VLOG(1) << "Receiver: Exiting.";
}

} // namespace

int RunMmapBenchmark(int num_iterations, int num_warmups, uint64_t data_size,
                     uint64_t buffer_size) {
  SenseReversingBarrier::ClearResource(BARRIER_ID);
  unlink(MMAP_FILE_PATH.c_str());

  pid_t pid = fork();

  if (pid == -1) {
    LOG(ERROR) << "fork: " << strerror(errno);
    return 1;
  }

  if (pid == 0) {
    SendProcess(num_warmups, num_iterations, data_size, buffer_size);
    exit(0);
  } else {
    ReceiveProcess(num_warmups, num_iterations, data_size);
    waitpid(pid, nullptr, 0);
    unlink(MMAP_FILE_PATH.c_str());
  }

  return 0;
}
