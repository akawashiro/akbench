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
    barrier
    rt
    pthread)

add_library(common common.cc)
target_link_libraries(common ${IPC_BENCH_LIBS})

set(IPC_BENCH_LIBS common ${IPC_BENCH_LIBS})

# Create benchmark libraries
add_library(memcpy_bandwidth memcpy_bandwidth.cc)
target_link_libraries(memcpy_bandwidth ${IPC_BENCH_LIBS})

add_library(memcpy_mt_bandwidth memcpy_mt_bandwidth.cc)
target_link_libraries(memcpy_mt_bandwidth ${IPC_BENCH_LIBS})

add_library(tcp_bandwidth tcp_bandwidth.cc)
target_link_libraries(tcp_bandwidth ${IPC_BENCH_LIBS})

add_library(uds_bandwidth uds_bandwidth.cc)
target_link_libraries(uds_bandwidth ${IPC_BENCH_LIBS})

add_library(pipe_bandwidth pipe_bandwidth.cc)
target_link_libraries(pipe_bandwidth ${IPC_BENCH_LIBS})

add_library(fifo_bandwidth fifo_bandwidth.cc)
target_link_libraries(fifo_bandwidth ${IPC_BENCH_LIBS})

add_library(mq_bandwidth mq_bandwidth.cc)
target_link_libraries(mq_bandwidth ${IPC_BENCH_LIBS} rt)

add_library(mmap_bandwidth mmap_bandwidth.cc)
target_link_libraries(mmap_bandwidth ${IPC_BENCH_LIBS})

add_library(shm_bandwidth shm_bandwidth.cc)
target_link_libraries(shm_bandwidth ${IPC_BENCH_LIBS})

add_executable(bandwidth bandwidth.cc)
target_link_libraries(
  bandwidth
  memcpy_bandwidth
  memcpy_mt_bandwidth
  tcp_bandwidth
  uds_bandwidth
  pipe_bandwidth
  fifo_bandwidth
  mq_bandwidth
  mmap_bandwidth
  shm_bandwidth
  ${IPC_BENCH_LIBS})

add_executable(barrier_test barrier_test.cc)
target_link_libraries(barrier_test ${IPC_BENCH_LIBS})
add_test(NAME barrier_test COMMAND barrier_test)

find_package(MPI REQUIRED COMPONENTS CXX)
add_executable(mpi_bandwidth mpi_bandwidth.cc)
target_link_libraries(mpi_bandwidth PRIVATE MPI::MPI_CXX ${IPC_BENCH_LIBS})

add_library(atomic_latency atomic_latency.cc)
target_link_libraries(atomic_latency ${IPC_BENCH_LIBS})

add_library(condition_variable_latency condition_variable_latency.cc)
target_link_libraries(condition_variable_latency ${IPC_BENCH_LIBS})

add_library(syscall_latency syscall_latency.cc)
target_link_libraries(syscall_latency ${IPC_BENCH_LIBS})

add_executable(latency latency.cc)
target_link_libraries(latency atomic_latency condition_variable_latency
                      syscall_latency ${IPC_BENCH_LIBS})
