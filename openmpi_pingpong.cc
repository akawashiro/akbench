#include <chrono> // High-precision timer from C++11 onwards
#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <numeric>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "common.h"

ABSL_FLAG(int, num_iterations, 100,
          "Number of measurement iterations (minimum 3)");
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

  // Validate num_iterations
  if (num_iterations < 3) {
    if (rank == 0) {
      LOG(ERROR) << "num_iterations must be at least 3, got: "
                 << num_iterations;
    }
    MPI_Finalize();
    return 1;
  }

  // Use the specified message size from command line
  const int msg_size = max_msg_size;

  // Allocate buffers
  std::vector<uint8_t> send_buffer;
  std::vector<uint8_t> recv_buffer(msg_size);

  if (rank == 0) {
    LOG(INFO) << "Message Size (Bytes)\tBandwidth" << GIBYTE_PER_SEC_UNIT;
    LOG(INFO) << "----------------------------------------";
  }

  // Generate data for the specified message size
  if (msg_size > CHECKSUM_SIZE) {
    send_buffer = GenerateDataToSend(msg_size);
  } else {
    // For small sizes, use simple pattern
    send_buffer.resize(msg_size);
    for (int i = 0; i < msg_size; ++i) {
      send_buffer[i] = static_cast<uint8_t>(i % 256);
    }
  }

  if (rank == 0) {
    VLOG(1) << "Testing message size: " << msg_size << " bytes";
  }

  MPI_Barrier(MPI_COMM_WORLD); // Wait until all processes synchronize

  // Warmup runs
  for (int i = 0; i < num_warmups; ++i) {
    if (rank == 0) {
      MPI_Send(send_buffer.data(), msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
      MPI_Recv(recv_buffer.data(), msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
    } else {
      MPI_Recv(recv_buffer.data(), msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
      MPI_Send(send_buffer.data(), msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }
  }

  MPI_Barrier(MPI_COMM_WORLD); // Sync after warmup

  std::vector<double> durations;

  for (int i = 0; i < num_iterations; ++i) {
    auto start_time = std::chrono::high_resolution_clock::now();

    if (rank == 0) {
      MPI_Send(send_buffer.data(), msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
      MPI_Recv(recv_buffer.data(), msg_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
    } else {
      MPI_Recv(recv_buffer.data(), msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
      MPI_Send(send_buffer.data(), msg_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;
    durations.push_back(duration.count());

    // Verify received data (only for messages with checksum)
    if (msg_size > CHECKSUM_SIZE) {
      if (!VerifyDataReceived(recv_buffer, msg_size)) {
        LOG(ERROR) << "Data verification failed for iteration " << i;
      }
    }
  }

  // Bandwidth calculation using CalculateBandwidth function
  // In ping-pong, 2 * msg_size bytes are transferred per iteration (round
  // trip)
  double bandwidth_bytes_per_sec =
      CalculateBandwidth(durations, num_iterations, 2 * msg_size);
  double bandwidth_gib_s = bandwidth_bytes_per_sec / (1024.0 * 1024.0 * 1024.0);

  if (rank == 0) {
    LOG(INFO) << msg_size << "\t\t\t" << std::fixed << std::setprecision(2)
              << bandwidth_gib_s;
  }

  MPI_Finalize();
  return 0;
}
