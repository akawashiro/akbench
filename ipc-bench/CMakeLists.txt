add_subdirectory(abseil-cpp)

add_library(barrier barrier.cc)
target_link_libraries(barrier absl::log)
set(IPC_BENCH_LIBS
    absl::log
    absl::check
    absl::base
    absl::synchronization
    absl::log_initialize
    absl::flags
    absl::flags_parse
    barrier)

add_library(common common.cc)
target_link_libraries(common ${IPC_BENCH_LIBS})

set(IPC_BENCH_LIBS common ${IPC_BENCH_LIBS})

# Create benchmark libraries
add_library(memcpy_benchmark memcpy_benchmark.cc)
target_link_libraries(memcpy_benchmark ${IPC_BENCH_LIBS})

add_library(memcpy_mt_benchmark memcpy_mt_benchmark.cc)
target_link_libraries(memcpy_mt_benchmark ${IPC_BENCH_LIBS})

add_library(tcp_benchmark tcp_benchmark.cc)
target_link_libraries(tcp_benchmark ${IPC_BENCH_LIBS})

add_library(uds_benchmark uds_benchmark.cc)
target_link_libraries(uds_benchmark ${IPC_BENCH_LIBS})

add_library(pipe_benchmark pipe_benchmark.cc)
target_link_libraries(pipe_benchmark ${IPC_BENCH_LIBS})

add_library(fifo_benchmark fifo_benchmark.cc)
target_link_libraries(fifo_benchmark ${IPC_BENCH_LIBS})

add_library(mq_benchmark mq_benchmark.cc)
target_link_libraries(mq_benchmark ${IPC_BENCH_LIBS} rt)

add_library(mmap_benchmark mmap_benchmark.cc)
target_link_libraries(mmap_benchmark ${IPC_BENCH_LIBS})

add_library(shm_benchmark shm_benchmark.cc)
target_link_libraries(shm_benchmark ${IPC_BENCH_LIBS} rt pthread)

add_executable(bandwidth bandwidth.cc)
target_link_libraries(
  bandwidth
  memcpy_benchmark
  memcpy_mt_benchmark
  tcp_benchmark
  uds_benchmark
  pipe_benchmark
  fifo_benchmark
  mq_benchmark
  mmap_benchmark
  shm_benchmark
  ${IPC_BENCH_LIBS}
  rt
  pthread)

add_executable(barrier_test barrier_test.cc)
target_link_libraries(barrier_test ${IPC_BENCH_LIBS} rt pthread)
add_test(NAME barrier_test COMMAND barrier_test)

find_package(MPI REQUIRED COMPONENTS CXX)
add_executable(mpi_bandwidth mpi_bandwidth.cc)
target_link_libraries(mpi_bandwidth PRIVATE MPI::MPI_CXX ${IPC_BENCH_LIBS})
