#include "tcp_bandwidth.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
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
const int PORT = 12345;
const std::string LOOPBACK_IP = "127.0.0.1";
const std::string BARRIER_ID = "/tcp_benchmark";

double ReceiveProcess(int num_warmups, int num_iterations, uint64_t data_size,
                      uint64_t buffer_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

  std::vector<double> durations;

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    int listen_fd, conn_fd;
    struct sockaddr_in receive_addr, send_addr;
    socklen_t send_len = sizeof(send_addr);

    bool is_warmup = iteration < num_warmups;

    if (is_warmup) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Warm-up {}/{}", ReceivePrefix(iteration), iteration,
                        num_warmups));
    }

    // Create a TCP socket for each iteration
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: socket: {}", strerror(errno)));
    }

    // Allow immediate reuse of the port after the program exits
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1) {
      AKLOG(
          aklog::LogLevel::FATAL,
          std::format("receive: setsockopt SO_REUSEADDR: {}", strerror(errno)));
    }

    // Configure receive address
    memset(&receive_addr, 0, sizeof(receive_addr));
    receive_addr.sin_family = AF_INET;
    receive_addr.sin_addr.s_addr = inet_addr(LOOPBACK_IP.c_str());
    receive_addr.sin_port = htons(PORT);

    // Bind the socket to the specified IP address and port
    if (bind(listen_fd, (struct sockaddr *)&receive_addr,
             sizeof(receive_addr)) == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: bind: {}", strerror(errno)));
    }

    // Listen for incoming connections
    if (listen(listen_fd, 5) == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: listen: {}", strerror(errno)));
    }

    barrier.Wait();
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("{}Listening on {}:{}", ReceivePrefix(iteration),
                      LOOPBACK_IP, PORT));

    // Accept a sender connection for this iteration
    conn_fd = accept(listen_fd, (struct sockaddr *)&send_addr, &send_len);
    if (conn_fd == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("receive: accept: {}", strerror(errno)));
    }

    if (!is_warmup) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Sender connected from {}:{}. Receiving data...",
                        ReceivePrefix(iteration), inet_ntoa(send_addr.sin_addr),
                        ntohs(send_addr.sin_port)));
    }

    std::vector<uint8_t> recv_buffer(buffer_size);
    std::vector<uint8_t> received_data(data_size);
    received_data.reserve(data_size);

    barrier.Wait();
    size_t total_received = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    // Receive data until data_size is reached
    while (total_received < data_size) {
      ssize_t bytes_received =
          recv(conn_fd, recv_buffer.data(), buffer_size, 0);
      if (bytes_received == -1) {
        AKLOG(aklog::LogLevel::FATAL,
              std::format("receive: recv: {}", strerror(errno)));
      }
      if (bytes_received == 0) {
        if (!is_warmup) {
          AKLOG(aklog::LogLevel::INFO,
                std::format("{}Sender disconnected prematurely.",
                            ReceivePrefix(iteration)));
        }
        break;
      }
      memcpy(received_data.data() + total_received, recv_buffer.data(),
             bytes_received);
      total_received += bytes_received;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());

      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Received {} GiB of data in {} ms.",
                        ReceivePrefix(iteration),
                        total_received / (1024.0 * 1024.0 * 1024.0),
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

    // Close connection and listening socket for this iteration
    close(conn_fd);
    close(listen_fd);
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
            std::format("{}Connecting to receiver at {}:{}",
                        SendPrefix(iteration), LOOPBACK_IP, PORT));
    }

    int sock_fd;
    struct sockaddr_in receive_addr;

    // Create a TCP socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("send: socket: {}", strerror(errno)));
    }

    // Configure receive address to connect to
    memset(&receive_addr, 0, sizeof(receive_addr));
    receive_addr.sin_family = AF_INET;
    receive_addr.sin_addr.s_addr = inet_addr(LOOPBACK_IP.c_str());
    receive_addr.sin_port = htons(PORT);

    // Wait until the receiver is ready
    barrier.Wait();

    while (connect(sock_fd, (struct sockaddr *)&receive_addr,
                   sizeof(receive_addr)) == -1) {
      if (!is_warmup) {
        AKLOG(aklog::LogLevel::ERROR,
              std::format("send: connect (retrying in 1 second): {}",
                          strerror(errno)));
      }
      sleep(1);
    }

    if (!is_warmup) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("{}Connected to receiver. Sending data...",
                        SendPrefix(iteration)));
    }

    barrier.Wait();
    size_t total_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_sent < data_size) {
      size_t bytes_to_send = std::min(buffer_size, data_size - total_sent);
      ssize_t bytes_sent =
          send(sock_fd, data_to_send.data() + total_sent, bytes_to_send, 0);
      if (bytes_sent == -1) {
        AKLOG(aklog::LogLevel::FATAL,
              std::format("send: send: {}", strerror(errno)));
      }
      total_sent += bytes_sent;
    }
    shutdown(sock_fd, SHUT_WR);
    auto end_time = std::chrono::high_resolution_clock::now();

    barrier.Wait();

    if (!is_warmup) {
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
        std::format("Send bandwidth: {}{}.", bandwidth / (1 << 30),
                    GIBYTE_PER_SEC_UNIT));
}

} // namespace

double RunTcpBandwidthBenchmark(int num_iterations, int num_warmups,
                                uint64_t data_size, uint64_t buffer_size) {
  SenseReversingBarrier::ClearResource(BARRIER_ID);

  pid_t pid = fork();
  if (pid == -1) {
    AKLOG(aklog::LogLevel::FATAL, std::format("fork: {}", strerror(errno)));
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
