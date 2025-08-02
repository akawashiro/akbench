#include <chrono>
#include <format>
#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <numeric>
#include <optional>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "aklog.h"
#include "common.h"

ABSL_FLAG(int, num_iterations, 10,
          "Number of measurement iterations (minimum 3)");
ABSL_FLAG(int, num_warmups, 3, "Number of warmup iterations");
ABSL_FLAG(uint64_t, data_size, 1024 * 1024, "Maximum message size in bytes");

int main(int argc, char **argv) {
  absl::SetProgramUsageMessage(
      "ping-pong benchmark tool for measuring MPI bandwidth.");
  absl::ParseCommandLine(argc, argv);
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (size != 2) {
    if (rank == 0) {
      AKLOG(aklog::LogLevel::ERROR,
            "Error: This program should be run with 2 MPI processes.");
    }
    MPI_Finalize();
    return 1;
  }

  const int num_iterations = absl::GetFlag(FLAGS_num_iterations);
  const int num_warmups = absl::GetFlag(FLAGS_num_warmups);
  const uint64_t data_size = absl::GetFlag(FLAGS_data_size);

  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  absl::InitializeLog();

  if (num_iterations < 3) {
    if (rank == 0) {
      AKLOG(aklog::LogLevel::ERROR,
            std::format("num_iterations must be at least 3, got: {}",
                        num_iterations));
    }
    MPI_Finalize();
    return 1;
  }

  if (data_size <= CHECKSUM_SIZE) {
    if (rank == 0) {
      AKLOG(aklog::LogLevel::ERROR,
            std::format("data_size must be greater than {}, got: {}",
                        CHECKSUM_SIZE, data_size));
    }
    MPI_Finalize();
    return 1;
  }

  std::vector<uint8_t> send_buffer = GenerateDataToSend(data_size);
  std::vector<uint8_t> recv_buffer(data_size);

  if (rank == 0) {
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("Testing message size: {} bytes", data_size));
  }

  MPI_Barrier(MPI_COMM_WORLD);

  std::vector<double> durations;

  for (int i = 0; i < num_warmups + num_iterations; ++i) {
    bool is_warmup = i < num_warmups;

    if (is_warmup && rank == 0) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("Warm-up {}/{}", i, num_warmups));
    } else if (!is_warmup && rank == 0) {
      AKLOG(aklog::LogLevel::DEBUG,
            std::format("Starting iteration {}/{}", (i - num_warmups),
                        num_iterations));
    }

    MPI_Barrier(MPI_COMM_WORLD);
    auto start_time = std::chrono::high_resolution_clock::now();
    if (rank == 0) {
      MPI_Send(send_buffer.data(), data_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
      MPI_Recv(recv_buffer.data(), data_size, MPI_BYTE, 1, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
    } else {
      MPI_Recv(recv_buffer.data(), data_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
      MPI_Send(send_buffer.data(), data_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    MPI_Barrier(MPI_COMM_WORLD);

    if (!is_warmup) {
      std::chrono::duration<double> duration = end_time - start_time;
      durations.push_back(duration.count());
    }

    if (!VerifyDataReceived(recv_buffer, data_size)) {
      AKLOG(aklog::LogLevel::FATAL,
            std::format("Data verification failed for iteration {}", i));
    }
  }

  double bandwidth =
      CalculateBandwidth(durations, num_iterations, 2 * data_size);

  if (rank == 0) {
    AKLOG(aklog::LogLevel::INFO,
          std::format("{}{}", bandwidth / (1 << 30), GIBYTE_PER_SEC_UNIT));
  }

  MPI_Finalize();
  return 0;
}
