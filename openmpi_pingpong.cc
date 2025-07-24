#include <chrono> // High-precision timer from C++11 onwards
#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "common.h"

ABSL_FLAG(int, num_iterations, 100, "Number of measurement iterations");
ABSL_FLAG(int, num_warmups, 10, "Number of warmup iterations");
ABSL_FLAG(uint64_t, data_size, 1024 * 1024, "Maximum message size in bytes");

int main(int argc, char **argv) {
  absl::SetProgramUsageMessage(
      "OpenMPI ping-pong benchmark tool for measuring MPI bandwidth.");
  absl::ParseCommandLine(argc, argv);
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (size != 2) {
    if (rank == 0) {
      LOG(ERROR) << "Error: This program should be run with 2 MPI processes.";
    }
    MPI_Finalize();
    return 1;
  }

  // Get values from command line flags
  const int num_iterations = absl::GetFlag(FLAGS_num_iterations);
  const int num_warmups = absl::GetFlag(FLAGS_num_warmups);
  const uint64_t max_msg_size = absl::GetFlag(FLAGS_data_size);

  // Allocate buffers
  // Using char type to handle any data type at byte level
  std::vector<char> send_buffer(max_msg_size);
  std::vector<char> recv_buffer(max_msg_size);

  // Initialize dummy data (optional but recommended)
  for (int i = 0; i < max_msg_size; ++i) {
    send_buffer[i] = static_cast<char>(i % 256);
  }

  if (rank == 0) {
    LOG(INFO) << "Message Size (Bytes)\tBandwidth (MB/s)";
    LOG(INFO) << "----------------------------------------";
  }

  for (int msg_size = 1; msg_size <= max_msg_size; msg_size *= 2) {
    if (msg_size > max_msg_size)
      msg_size = max_msg_size; // Adjust to exact size on the last loop

    if (rank == 0) {
      VLOG(1) << "Testing message size: " << msg_size << " bytes";
    }

    MPI_Barrier(MPI_COMM_WORLD); // Wait until all processes synchronize

    // Warmup runs
    for (int i = 0; i < num_warmups; ++i) {
      if (rank == 0) {
        MPI_Send(send_buffer.data(), msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(recv_buffer.data(), msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
      } else {
        MPI_Recv(recv_buffer.data(), msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        MPI_Send(send_buffer.data(), msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
      }
    }

    MPI_Barrier(MPI_COMM_WORLD); // Sync after warmup

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
      if (rank == 0) {
        MPI_Send(send_buffer.data(), msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(recv_buffer.data(), msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
      } else {
        MPI_Recv(recv_buffer.data(), msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        MPI_Send(send_buffer.data(), msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
      }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_time = end_time - start_time;

    // Bandwidth calculation (MB/s)
    // In one ping-pong, 2 * msg_size bytes are transferred (round trip)
    // total_time is the time for num_iterations
    // Therefore, total data transferred = num_iterations * 2 * msg_size bytes
    // Bandwidth = (total data transferred / total_time.count()) converted to
    // MB/s
    double bandwidth_mb_s = (double)(num_iterations * 2 * msg_size) /
                            total_time.count() / (1024.0 * 1024.0);

    if (rank == 0) {
      LOG(INFO) << msg_size << "\t\t\t" << std::fixed << std::setprecision(2)
                << bandwidth_mb_s;
    }

    if (msg_size == max_msg_size && msg_size % 2 != 0)
      break; // Exit loop after last iteration (countermeasure for when msg_size
             // is 1)
  }

  MPI_Finalize();
  return 0;
}
