#include "mq_bandwidth.h"

#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <vector>

#include "aklog.h"

#include "barrier.h"
#include "common.h"

namespace {

const std::string BARRIER_ID = GenerateUniqueBarrierId("/mq_benchmark");
const std::string MQ_NAME = GenerateUniqueBarrierId("/mq_benchmark_queue");

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
            std::format("{}Starting iteration {}/{}", SendPrefix(iteration),
                        iteration, num_iterations));
    }

    // Open message queue for writing
    mqd_t mq = mq_open(MQ_NAME.c_str(), O_WRONLY);
    if (mq == (mqd_t)-1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("send: mq_open for writing: {}", strerror(errno)));
    }

    barrier.Wait();
    size_t total_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_sent < data_size) {
      size_t bytes_to_send = std::min(buffer_size, data_size - total_sent);

      // POSIX message queues have size limits, so we send in chunks
      int ret = mq_send(
          mq, reinterpret_cast<const char *>(data_to_send.data() + total_sent),
          bytes_to_send, 0);
      if (ret == -1) {
        AKLOG(aklog::LogLevel::FATAL,
              std::format("send: mq_send: {}", strerror(errno)));
      }
      total_sent += bytes_to_send;
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Time taken: {} ms.", SendPrefix(iteration),
                        elapsed_time.count() * 1000));
    }

    mq_close(mq);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);

  double bandwidth_gibps = bandwidth / (1024.0 * 1024.0 * 1024.0);
  AKLOG(aklog::LogLevel::INFO,
        std::format("Send bandwidth: {}{}.", bandwidth_gibps,
                    GIBYTE_PER_SEC_UNIT));

  AKLOG(aklog::LogLevel::DEBUG, std::format("{}Exiting.", SendPrefix(-1)));
}

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
            std::format("{}Starting iteration {}/{}", ReceivePrefix(iteration),
                        iteration, num_iterations));
    }

    std::vector<uint8_t> recv_buffer(buffer_size);
    std::vector<uint8_t> received_data;
    received_data.reserve(data_size);

    // Open message queue for reading
    mqd_t mq = mq_open(MQ_NAME.c_str(), O_RDONLY);
    if (mq == (mqd_t)-1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: mq_open for reading: {}", strerror(errno)));
    }

    barrier.Wait();
    size_t total_received = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_received < data_size) {
      ssize_t bytes_read =
          mq_receive(mq, reinterpret_cast<char *>(recv_buffer.data()),
                     recv_buffer.size(), nullptr);
      if (bytes_read == -1) {
        if (errno == EAGAIN || errno == ETIMEDOUT) {
          // No more messages, sender may have finished
          break;
        }
        AKLOG(aklog::LogLevel::FATAL,
              std::format("receive: mq_receive: {}", strerror(errno)));
      }
      if (bytes_read == 0) {
        if (!is_warmup) {
          AKLOG(aklog::LogLevel::DEBUG,
                std::format("{}No more messages from sender.",
                            ReceivePrefix(iteration)));
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

      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Time taken: {} ms.", ReceivePrefix(iteration),
                        elapsed_time.count() * 1000));
    }

    if (!VerifyDataReceived(received_data, data_size)) {
      AKLOG(aklog::LogLevel::FATAL, std::format("{}Data verification failed!",
                                                ReceivePrefix(iteration)));
    } else {
      AKLOG(aklog::LogLevel::DEBUG, std::format("{}Data verification passed.",
                                                ReceivePrefix(iteration)));
    }

    mq_close(mq);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  AKLOG(aklog::LogLevel::INFO,
        std::format("Receive bandwidth: {}{}.", bandwidth / (1 << 30),
                    GIBYTE_PER_SEC_UNIT));

  AKLOG(aklog::LogLevel::DEBUG, std::format("{}Exiting.", ReceivePrefix(-1)));

  return bandwidth;
}

} // namespace

double RunMqBandwidthBenchmark(int num_iterations, int num_warmups,
                               uint64_t data_size, uint64_t buffer_size) {
  // Remove any existing message queue
  mq_unlink(MQ_NAME.c_str());

  // Limit message size to system limits (typically 8192 bytes)
  const size_t max_msg_size =
      std::min(buffer_size, static_cast<uint64_t>(8192));

  // Create message queue attributes
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;            // Maximum number of messages
  attr.mq_msgsize = max_msg_size; // Maximum message size
  attr.mq_curmsgs = 0;

  // Create message queue
  mqd_t mq = mq_open(MQ_NAME.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666, &attr);
  if (mq == (mqd_t)-1) {
    AKLOG(aklog::LogLevel::FATAL,
          std::format("mq_open (create): {}", strerror(errno)));
  }
  mq_close(mq);

  pid_t pid = fork();

  if (pid == -1) {
    AKLOG(aklog::LogLevel::FATAL, std::format("fork: {}", strerror(errno)));
  }

  if (pid == 0) {
    // Child process: sender
    SendProcess(num_warmups, num_iterations, data_size, max_msg_size);
    exit(0);
  } else {
    // Parent process: receiver
    double bandwidth =
        ReceiveProcess(num_warmups, num_iterations, data_size, max_msg_size);
    waitpid(pid, nullptr, 0);

    // Clean up message queue
    mq_unlink(MQ_NAME.c_str());
    SenseReversingBarrier::ClearResource(BARRIER_ID);

    return bandwidth;
  }
}
