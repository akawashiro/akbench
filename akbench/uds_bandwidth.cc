#include "uds_bandwidth.h"

#include <sys/socket.h>
#include <sys/un.h>
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

const std::string SOCKET_PATH =
    GenerateUniqueName("/tmp/unix_domain_socket_test.sock");
constexpr size_t DEFAULT_BUFFER_SIZE = (1 << 20);
const std::string BARRIER_ID = GenerateUniqueName("/uds_benchmark");

double ReceiveProcess(uint64_t buffer_size, int num_warmups, int num_iterations,
                      uint64_t data_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

  std::vector<double> durations;
  std::vector<uint8_t> read_data(data_size, 0x00);

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    int listen_fd, conn_fd;
    struct sockaddr_un addr;

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    AKCHECK(listen_fd != -1, "Failed to create socket");

    remove(SOCKET_PATH.c_str());

    // Configure the socket address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

    // Bind the socket to the specified path
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("Failed to bind socket to {}", SOCKET_PATH));
    }

    // Listen for incoming connections
    if (listen(listen_fd, 0) == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("Failed to listen on socket {}", SOCKET_PATH));
    }

    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Waiting for sender connection on {}",
                      ReceivePrefix(iteration), SOCKET_PATH));

    conn_fd = accept(listen_fd, NULL, NULL);
    AKCHECK(conn_fd != -1, "Failed to accept connection");

    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Sender connected.", ReceivePrefix(iteration)));
    std::vector<uint8_t> recv_buffer(buffer_size);
    barrier.Wait();
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Begin receiving data.", ReceivePrefix(iteration)));
    size_t total_received = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_received < data_size) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Receiving data, total received: {} bytes.",
                        ReceivePrefix(iteration), total_received));
      ssize_t bytes_received =
          recv(conn_fd, recv_buffer.data(), buffer_size, 0);
      AKCHECK(bytes_received >= 0, "Failed to receive data");
      if (bytes_received == 0) {
        AKLOG(aklog::LogLevel::DEBUG,
              std::format("{}Sender disconnected prematurely.",
                          ReceivePrefix(iteration)));
        break;
      }
      total_received += bytes_received;
      memcpy(read_data.data() + total_received - bytes_received,
             recv_buffer.data(), bytes_received);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();
    close(conn_fd);
    close(listen_fd);
    remove(SOCKET_PATH.c_str());
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Finished receiving data.", ReceivePrefix(iteration)));

    VerifyDataReceived(read_data, data_size);
    if (num_warmups <= iteration) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Time taken: {} ms.", ReceivePrefix(iteration),
                        elapsed_time.count() * 1000));
    }
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  AKLOG(aklog::LogLevel::INFO,
        std::format(" Receive bandwidth: {}{}.", bandwidth / (1 << 30),
                    GIBYTE_PER_SEC_UNIT));

  return bandwidth;
}

void SendProcess(uint64_t buffer_size, int num_warmups, int num_iterations,
                 uint64_t data_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

  std::vector<uint8_t> data_to_send = GenerateDataToSend(data_size);
  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    int sock_fd;
    struct sockaddr_un addr;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    AKCHECK(sock_fd != -1, "Failed to create socket");

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Connecting to receiver on {}", SendPrefix(iteration),
                      SOCKET_PATH));
    while (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      if (errno == ENOENT || errno == ECONNREFUSED) {
        AKLOG(aklog::LogLevel::DEBUG,
              std::format("{}Connection failed: {}. Retrying...",
                          SendPrefix(iteration), strerror(errno)));
        sleep(1);
      } else {
        AKLOG(aklog::LogLevel::FATAL,
              std::format("{}Unexpected error: {}", SendPrefix(iteration),
                          strerror(errno)));
      }
    }

    barrier.Wait();
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Begin data transfer.", SendPrefix(iteration)));
    size_t total_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_sent < data_size) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Sending data, total sent: {} bytes.",
                        SendPrefix(iteration), total_sent));
      size_t bytes_to_send = std::min(buffer_size, data_size - total_sent);
      ssize_t bytes_sent =
          send(sock_fd, data_to_send.data() + total_sent, bytes_to_send, 0);
      if (bytes_sent == -1) {
        AKLOG(aklog::LogLevel::FATAL,
              std::format("Send: Failed to send data: {}", strerror(errno)));
        break;
      }
      total_sent += bytes_sent;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Finish data transfer", SendPrefix(iteration)));

    if (num_warmups <= iteration) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Time taken: {} ms.", SendPrefix(iteration),
                        elapsed_time.count() * 1000));
    }
    close(sock_fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  AKLOG(aklog::LogLevel::INFO,
        std::format(" Send bandwidth: {}{}.", bandwidth / (1 << 30),
                    GIBYTE_PER_SEC_UNIT));
}

double RunUdsBandwidthBenchmark(int num_iterations, int num_warmups,
                                uint64_t data_size, uint64_t buffer_size) {
  SenseReversingBarrier::ClearResource(BARRIER_ID);

  pid_t pid = fork();
  AKCHECK(pid != -1, "Failed to fork process");

  if (pid == 0) {
    SendProcess(buffer_size, num_warmups, num_iterations, data_size);
    exit(0);
  } else {
    double bandwidth =
        ReceiveProcess(buffer_size, num_warmups, num_iterations, data_size);
    waitpid(pid, nullptr, 0);
    return bandwidth;
  }
}
