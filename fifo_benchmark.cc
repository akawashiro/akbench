#include "fifo_benchmark.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "absl/log/log.h"

#include "barrier.h"
#include "common.h"

namespace {

const std::string BARRIER_ID = "/fifo_benchmark";
const std::string FIFO_PATH = "/tmp/fifo_benchmark_pipe";

void SendProcess(int num_warmups, int num_iterations, uint64_t data_size,
                 uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

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

    // Open FIFO for writing
    int write_fd = open(FIFO_PATH.c_str(), O_WRONLY);
    if (write_fd == -1) {
      LOG(ERROR) << "send: open FIFO for writing: " << strerror(errno);
      return;
    }

    barrier.Wait();
    size_t total_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_sent < data_size) {
      size_t bytes_to_send = std::min(buffer_size, data_size - total_sent);
      ssize_t bytes_written =
          write(write_fd, data_to_send.data() + total_sent, bytes_to_send);
      if (bytes_written == -1) {
        LOG(ERROR) << "send: write: " << strerror(errno);
        break;
      }
      total_sent += bytes_written;
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      VLOG(1) << "Sender: Time taken: " << elapsed_time.count() * 1000
              << " ms.";
    }

    close(write_fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);

  double bandwidth_gibps = bandwidth / (1024.0 * 1024.0 * 1024.0);
  LOG(INFO) << "Send bandwidth: " << bandwidth_gibps << " GiByte/sec.";

  VLOG(1) << "Sender: Exiting.";
}

void ReceiveProcess(int num_warmups, int num_iterations, uint64_t data_size,
                    uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    bool is_warmup = iteration < num_warmups;

    if (is_warmup) {
      VLOG(1) << "Receiver: Warm-up " << iteration << "/" << num_warmups;
    } else {
      VLOG(1) << "Receiver: Starting iteration " << iteration << "/"
              << num_iterations;
    }

    std::vector<uint8_t> recv_buffer(buffer_size);
    std::vector<uint8_t> received_data;
    received_data.reserve(data_size);

    // Open FIFO for reading
    int read_fd = open(FIFO_PATH.c_str(), O_RDONLY);
    if (read_fd == -1) {
      LOG(ERROR) << "receive: open FIFO for reading: " << strerror(errno);
      return;
    }

    barrier.Wait();
    size_t total_received = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_received < data_size) {
      ssize_t bytes_read = read(read_fd, recv_buffer.data(), buffer_size);
      if (bytes_read == -1) {
        LOG(ERROR) << "receive: read: " << strerror(errno);
        break;
      }
      if (bytes_read == 0) {
        if (!is_warmup) {
          VLOG(1) << "Receiver: Sender closed the FIFO prematurely.";
        }
        break;
      }
      total_received += bytes_read;
      received_data.insert(received_data.end(), recv_buffer.begin(),
                           recv_buffer.begin() + bytes_read);
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());

      VLOG(1) << "Receiver: Time taken: " << elapsed_time.count() * 1000
              << " ms.";
    }

    if (!VerifyDataReceived(received_data, data_size)) {
      LOG(ERROR) << ReceivePrefix(iteration) << "Data verification failed!";
    } else {
      VLOG(1) << ReceivePrefix(iteration) << "Data verification passed.";
    }

    close(read_fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  LOG(INFO) << "Receive bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";

  VLOG(1) << "Receiver: Exiting.";
}

} // namespace

int RunFifoBenchmark(int num_iterations, int num_warmups, uint64_t data_size,
                     uint64_t buffer_size) {
  // Remove any existing FIFO
  unlink(FIFO_PATH.c_str());

  // Create FIFO
  if (mkfifo(FIFO_PATH.c_str(), 0666) == -1) {
    LOG(ERROR) << "mkfifo: " << strerror(errno);
    return 1;
  }

  pid_t pid = fork();

  if (pid == -1) {
    LOG(ERROR) << "fork: " << strerror(errno);
    unlink(FIFO_PATH.c_str());
    return 1;
  }

  if (pid == 0) {
    // Child process: sender
    SendProcess(num_warmups, num_iterations, data_size, buffer_size);
    exit(0);
  } else {
    // Parent process: receiver
    ReceiveProcess(num_warmups, num_iterations, data_size, buffer_size);
    waitpid(pid, nullptr, 0);
  }

  // Clean up FIFO
  unlink(FIFO_PATH.c_str());

  return 0;
}
