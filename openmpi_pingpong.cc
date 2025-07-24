#include <chrono> // High-precision timer from C++11 onwards
#include <iostream>
#include <mpi.h>
#include <vector>
#include <iomanip>

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (size != 2) {
    if (rank == 0) {
      std::cerr
          << "Error: This program should be run with 2 MPI processes."
          << std::endl;
    }
    MPI_Finalize();
    return 1;
  }

  const int num_iterations = 100; // Number of iterations for each message size
  const int max_msg_size = 1024 * 1024; // Maximum message size (1 MB)

  // Allocate buffers
  // Using char type to handle any data type at byte level
  std::vector<char> send_buffer(max_msg_size);
  std::vector<char> recv_buffer(max_msg_size);

  // Initialize dummy data (optional but recommended)
  for (int i = 0; i < max_msg_size; ++i) {
    send_buffer[i] = static_cast<char>(i % 256);
  }

  if (rank == 0) {
    std::cout << "Message Size (Bytes)\tBandwidth (MB/s)" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
  }

  for (int msg_size = 1; msg_size <= max_msg_size; msg_size *= 2) {
    if (msg_size > max_msg_size)
      msg_size = max_msg_size; // Adjust to exact size on the last loop

    MPI_Barrier(MPI_COMM_WORLD); // Wait until all processes synchronize

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
    // Bandwidth = (total data transferred / total_time.count()) converted to MB/s
    double bandwidth_mb_s = (double)(num_iterations * 2 * msg_size) /
                            total_time.count() / (1024.0 * 1024.0);

    if (rank == 0) {
      std::cout << msg_size << "\t\t\t" << std::fixed << std::setprecision(2)
                << bandwidth_mb_s << std::endl;
    }

    if (msg_size == max_msg_size && msg_size % 2 != 0)
      break; // Exit loop after last iteration (countermeasure for when msg_size is 1)
  }

  MPI_Finalize();
  return 0;
}
