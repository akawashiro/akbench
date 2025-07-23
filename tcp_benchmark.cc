#include "tcp_benchmark.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "absl/log/log.h"

#include "barrier.h"
#include "common.h"

namespace {
const int PORT = 12345;
const std::string LOOPBACK_IP = "127.0.0.1";
const std::string BARRIER_ID = "/tcp_benchmark";

void ReceiveProcess(int num_warmups, int num_iterations, uint64_t data_size,
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
      VLOG(1) << "Receiver: Warm-up " << iteration << "/" << num_warmups;
    }

    // Create a TCP socket for each iteration
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
      LOG(ERROR) << "receive: socket: " << strerror(errno);
      return;
    }

    // Allow immediate reuse of the port after the program exits
    int optval = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1) {
      LOG(ERROR) << "receive: setsockopt SO_REUSEADDR: " << strerror(errno);
      close(listen_fd);
      return;
    }

    // Configure receive address
    memset(&receive_addr, 0, sizeof(receive_addr));
    receive_addr.sin_family = AF_INET;
    receive_addr.sin_addr.s_addr = inet_addr(LOOPBACK_IP.c_str());
    receive_addr.sin_port = htons(PORT);

    // Bind the socket to the specified IP address and port
    if (bind(listen_fd, (struct sockaddr *)&receive_addr,
             sizeof(receive_addr)) == -1) {
      LOG(ERROR) << "receive: bind: " << strerror(errno);
      close(listen_fd);
      return;
    }

    // Listen for incoming connections
    if (listen(listen_fd, 5) == -1) {
      LOG(ERROR) << "receive: listen: " << strerror(errno);
      close(listen_fd);
      return;
    }

    barrier.Wait();
    VLOG(1) << ReceivePrefix(iteration) << "Listening on " << LOOPBACK_IP << ":"
            << PORT;

    // Accept a sender connection for this iteration
    conn_fd = accept(listen_fd, (struct sockaddr *)&send_addr, &send_len);
    if (conn_fd == -1) {
      LOG(ERROR) << "receive: accept: " << strerror(errno);
      close(listen_fd);
      return;
    }

    if (!is_warmup) {
      VLOG(1) << ReceivePrefix(iteration) << "Sender connected from "
              << inet_ntoa(send_addr.sin_addr) << ":"
              << ntohs(send_addr.sin_port) << ". Receiving data...";
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
        LOG(ERROR) << "receive: recv: " << strerror(errno);
        break;
      }
      if (bytes_received == 0) {
        if (!is_warmup) {
          LOG(INFO) << "Receiver: Sender disconnected prematurely.";
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

      VLOG(1) << "Receiver: Received "
              << total_received / (1024.0 * 1024.0 * 1024.0)
              << " GiB of data in " << elapsed_time.count() * 1000 << " ms.";
    }

    // Verify received data (always, even during warmup)
    if (!VerifyDataReceived(received_data, data_size)) {
      LOG(ERROR) << ReceivePrefix(iteration) << "Data verification failed!";
    } else {
      VLOG(1) << ReceivePrefix(iteration) << "Data verification passed.";
    }

    // Close connection and listening socket for this iteration
    close(conn_fd);
    close(listen_fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);

  LOG(INFO) << "Receive bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";
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
      VLOG(1) << "Sender: Warm-up " << iteration << "/" << num_warmups;
    } else {
      VLOG(1) << SendPrefix(iteration) << "Connecting to receiver at "
              << LOOPBACK_IP << ":" << PORT;
    }

    int sock_fd;
    struct sockaddr_in receive_addr;

    // Create a TCP socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
      LOG(ERROR) << "send: socket: " << strerror(errno);
      return;
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
        LOG(ERROR) << "send: connect (retrying in 1 second): "
                   << strerror(errno);
      }
      sleep(1);
    }

    if (!is_warmup) {
      VLOG(1) << SendPrefix(iteration)
              << "Connected to receiver. Sending data...";
    }

    barrier.Wait();
    size_t total_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_sent < data_size) {
      size_t bytes_to_send = std::min(buffer_size, data_size - total_sent);
      ssize_t bytes_sent =
          send(sock_fd, data_to_send.data() + total_sent, bytes_to_send, 0);
      if (bytes_sent == -1) {
        LOG(ERROR) << "send: send: " << strerror(errno);
        break;
      }
      total_sent += bytes_sent;
    }
    shutdown(sock_fd, SHUT_WR);
    auto end_time = std::chrono::high_resolution_clock::now();

    barrier.Wait();

    if (!is_warmup) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      VLOG(1) << "Sender: Time taken: " << elapsed_time.count() * 1000
              << " ms.";
    }

    close(sock_fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);

  LOG(INFO) << "Send bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";
}

} // namespace

int RunTcpBenchmark(int num_iterations, int num_warmups, uint64_t data_size,
                    uint64_t buffer_size) {
  SenseReversingBarrier::ClearResource(BARRIER_ID);

  pid_t pid = fork();
  if (pid == -1) {
    LOG(ERROR) << "fork: " << strerror(errno);
    return 1;
  }

  if (pid == 0) {
    SendProcess(num_warmups, num_iterations, data_size, buffer_size);
    exit(0);
  } else {
    ReceiveProcess(num_warmups, num_iterations, data_size, buffer_size);
    waitpid(pid, nullptr, 0);
  }

  return 0;
}
