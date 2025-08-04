#include <chrono>
#include <cstdlib>
#include <format>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <mpi.h>
#include <numeric>
#include <optional>
#include <vector>

#include "aklog.h"
#include "common.h"
#include "getopt_utils.h"

// Command line option variables
static int g_num_iterations = 10;
static int g_num_warmups = 3;
static uint64_t g_data_size = 1024 * 1024; // 1MB default

void print_usage(const char *program_name) {
  std::cout << R"(Usage: )" << program_name << R"( [OPTIONS]

Ping-pong benchmark tool for measuring MPI bandwidth.

Options:
  -i, --num-iterations=N   Number of measurement iterations (min 3, default: 10)
  -w, --num-warmups=N      Number of warmup iterations (default: 3)
  -d, --data-size=SIZE     Maximum message size in bytes (default: 1MB)
  -h, --help               Display this help message
)";
}

int main(int argc, char **argv) {
  const char *program_name = argv[0];

  // Define long options
  static struct option long_options[] = {
      {"num-iterations", required_argument, nullptr, 'i'},
      {"num-warmups", required_argument, nullptr, 'w'},
      {"data-size", required_argument, nullptr, 'd'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  // Parse command line options before MPI_Init
  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "i:w:d:h", long_options,
                            &option_index)) != -1) {
    try {
      switch (opt) {
      case 'i':
        g_num_iterations = parse_int(optarg);
        break;
      case 'w':
        g_num_warmups = parse_int(optarg);
        break;
      case 'd':
        g_data_size = parse_uint64(optarg).value();
        break;
      case 'h':
        print_usage(program_name);
        return 0;
      case '?':
        // getopt_long already printed an error message
        return 1;
      default:
        print_error_and_exit(program_name, "Unknown option");
      }
    } catch (const std::exception &e) {
      print_error_and_exit(program_name, e.what());
    }
  }

  // Initialize MPI after parsing arguments
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

  const int num_iterations = g_num_iterations;
  const int num_warmups = g_num_warmups;
  const uint64_t data_size = g_data_size;

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

  BenchmarkResult result =
      CalculateBandwidth(durations, num_iterations, 2 * data_size);

  if (rank == 0) {
    AKLOG(aklog::LogLevel::INFO,
          std::format("{:.3f} ± {:.3f}{}", result.average / (1 << 30),
                      result.stddev / (1 << 30), GIBYTE_PER_SEC_UNIT));
  }

  MPI_Finalize();
  return 0;
}
