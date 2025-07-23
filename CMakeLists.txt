cmake_minimum_required(VERSION 3.20)
project(ipc-bench LANGUAGES CXX)
enable_testing()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(abseil-cpp)

add_library(barrier barrier.cc)
target_link_libraries(barrier absl::log)
set(BANDWIDTH_LIBS
    absl::log
    absl::check
    absl::base
    absl::synchronization
    absl::log_initialize
    absl::flags
    absl::flags_parse
    barrier)

add_library(common common.cc)
target_link_libraries(common ${BANDWIDTH_LIBS})

set(BANDWIDTH_LIBS common ${BANDWIDTH_LIBS})

# Create benchmark libraries
add_library(memcpy_benchmark memcpy_benchmark.cc)
target_link_libraries(memcpy_benchmark ${BANDWIDTH_LIBS})

add_library(memcpy_mt_benchmark memcpy_mt_benchmark.cc)
target_link_libraries(memcpy_mt_benchmark ${BANDWIDTH_LIBS})

add_library(tcp_benchmark tcp_benchmark.cc)
target_link_libraries(tcp_benchmark ${BANDWIDTH_LIBS})

add_library(uds_benchmark uds_benchmark.cc)
target_link_libraries(uds_benchmark ${BANDWIDTH_LIBS})

add_library(pipe_benchmark pipe_benchmark.cc)
target_link_libraries(pipe_benchmark ${BANDWIDTH_LIBS})

add_library(fifo_benchmark fifo_benchmark.cc)
target_link_libraries(fifo_benchmark ${BANDWIDTH_LIBS})

add_library(mmap_benchmark mmap_benchmark.cc)
target_link_libraries(mmap_benchmark ${BANDWIDTH_LIBS})

add_library(shm_benchmark shm_benchmark.cc)
target_link_libraries(shm_benchmark ${BANDWIDTH_LIBS} rt pthread)

# Create the main bandwidth executable
add_executable(bandwidth bandwidth.cc)
target_link_libraries(
  bandwidth
  memcpy_benchmark
  memcpy_mt_benchmark
  tcp_benchmark
  uds_benchmark
  pipe_benchmark
  fifo_benchmark
  mmap_benchmark
  shm_benchmark
  ${BANDWIDTH_LIBS}
  rt
  pthread)

add_executable(barrier_test barrier_test.cc)
target_link_libraries(barrier_test ${BANDWIDTH_LIBS} rt pthread)
add_test(NAME barrier_test COMMAND barrier_test)
