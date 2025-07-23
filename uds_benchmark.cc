#include "uds_benchmark.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"

#include "barrier.h"
#include "common.h"

const std::string SOCKET_PATH = "/tmp/unix_domain_socket_test.sock";
constexpr size_t DEFAULT_BUFFER_SIZE = (1 << 20);
const std::string BARRIER_ID = "/uds_benchmark";

void ReceiveProcess(uint64_t buffer_size, int num_warmups, int num_iterations,
                    uint64_t data_size) {
  SenseReversingBarrier barrier(2, BARRIER_ID);

  std::vector<double> durations;
  std::vector<uint8_t> read_data(data_size, 0x00);

  for (int iteration = 0; iteration < num_warmups + num_iterations;
       ++iteration) {
    int listen_fd, conn_fd;
    struct sockaddr_un addr;

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(listen_fd != -1) << "Failed to create socket";

    remove(SOCKET_PATH.c_str());

    // Configure the socket address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

    // Bind the socket to the specified path
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      LOG(ERROR) << "Failed to bind socket to " << SOCKET_PATH;
      close(listen_fd);
      return;
    }

    // Listen for incoming connections
    if (listen(listen_fd, 0) == -1) {
      LOG(ERROR) << "Failed to listen on socket " << SOCKET_PATH;
      close(listen_fd);
      return;
    }

    VLOG(1) << ReceivePrefix(iteration) << "Waiting for sender connection on "
            << SOCKET_PATH;

    conn_fd = accept(listen_fd, NULL, NULL);
    CHECK(conn_fd != -1) << "Failed to accept connection";

    VLOG(1) << ReceivePrefix(iteration) << "Sender connected.";
    std::vector<uint8_t> recv_buffer(buffer_size);
    barrier.Wait();
    VLOG(1) << ReceivePrefix(iteration) << "Begin receiving data.";
    size_t total_received = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_received < data_size) {
      VLOG(1) << ReceivePrefix(iteration)
              << "Receiving data, total received: " << total_received
              << " bytes.";
      ssize_t bytes_received =
          recv(conn_fd, recv_buffer.data(), buffer_size, 0);
      CHECK(bytes_received >= 0) << "Failed to receive data";
      if (bytes_received == 0) {
        VLOG(1) << ReceivePrefix(iteration)
                << "Sender disconnected prematurely.";
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
    VLOG(1) << ReceivePrefix(iteration) << "Finished receiving data.";

    VerifyDataReceived(read_data, data_size);
    if (num_warmups <= iteration) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      VLOG(1) << ReceivePrefix(iteration)
              << "Time taken: " << elapsed_time.count() * 1000 << " ms.";
    }
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  LOG(INFO) << " Receive bandwidth: " << bandwidth / (1 << 30)
            << " GiByte/sec.";
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
    CHECK(sock_fd != -1) << "Failed to create socket";

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH.c_str(), sizeof(addr.sun_path) - 1);

    VLOG(1) << SendPrefix(iteration) << "Connecting to receiver on "
            << SOCKET_PATH;
    while (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      if (errno == ENOENT || errno == ECONNREFUSED) {
        VLOG(1) << SendPrefix(iteration)
                << "Connection failed: " << strerror(errno) << ". Retrying...";
        sleep(1);
      } else {
        LOG(ERROR) << SendPrefix(iteration)
                   << "Unexpected error: " << strerror(errno);
        close(sock_fd);
        return;
      }
    }

    barrier.Wait();
    VLOG(1) << SendPrefix(iteration) << "Begin data transfer.";
    size_t total_sent = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_sent < data_size) {
      VLOG(1) << SendPrefix(iteration)
              << "Sending data, total sent: " << total_sent << " bytes.";
      size_t bytes_to_send = std::min(buffer_size, data_size - total_sent);
      ssize_t bytes_sent =
          send(sock_fd, data_to_send.data() + total_sent, bytes_to_send, 0);
      if (bytes_sent == -1) {
        LOG(ERROR) << "Send: Failed to send data: " << strerror(errno);
        break;
      }
      total_sent += bytes_sent;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    barrier.Wait();
    VLOG(1) << SendPrefix(iteration) << "Finish data transfer";

    if (num_warmups <= iteration) {
      std::chrono::duration<double> elapsed_time = end_time - start_time;
      durations.push_back(elapsed_time.count());
      VLOG(1) << SendPrefix(iteration)
              << "Time taken: " << elapsed_time.count() * 1000 << " ms.";
    }
    close(sock_fd);
  }

  double bandwidth = CalculateBandwidth(durations, num_iterations, data_size);
  LOG(INFO) << " Send bandwidth: " << bandwidth / (1 << 30) << " GiByte/sec.";
}

int RunUdsBenchmark(int num_iterations, int num_warmups, uint64_t data_size,
                    uint64_t buffer_size) {
  SenseReversingBarrier::ClearResource(BARRIER_ID);

  pid_t pid = fork();
  CHECK(pid != -1) << "Failed to fork process";

  if (pid == 0) {
    SendProcess(buffer_size, num_warmups, num_iterations, data_size);
    exit(0);
  } else {
    ReceiveProcess(buffer_size, num_warmups, num_iterations, data_size);
    waitpid(pid, nullptr, 0);
  }

  return 0;
}
