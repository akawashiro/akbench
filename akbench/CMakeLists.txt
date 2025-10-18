add_library(aklog aklog.cc)

add_library(barrier barrier.cc)
target_link_libraries(barrier aklog)
set(AKBENCH_LIBS aklog barrier rt pthread)

add_library(common common.cc)
target_link_libraries(common ${AKBENCH_LIBS})

set(AKBENCH_LIBS common ${AKBENCH_LIBS})

# Create benchmark libraries
add_library(memcpy_bandwidth memcpy_bandwidth.cc)
target_link_libraries(memcpy_bandwidth ${AKBENCH_LIBS})

add_library(memcpy_mt_bandwidth memcpy_mt_bandwidth.cc)
target_link_libraries(memcpy_mt_bandwidth ${AKBENCH_LIBS})

add_library(tcp_bandwidth tcp_bandwidth.cc)
target_link_libraries(tcp_bandwidth ${AKBENCH_LIBS})

add_library(uds_bandwidth uds_bandwidth.cc)
target_link_libraries(uds_bandwidth ${AKBENCH_LIBS})

add_library(pipe_bandwidth pipe_bandwidth.cc)
target_link_libraries(pipe_bandwidth ${AKBENCH_LIBS})

add_library(fifo_bandwidth fifo_bandwidth.cc)
target_link_libraries(fifo_bandwidth ${AKBENCH_LIBS})

add_library(mq_bandwidth mq_bandwidth.cc)
target_link_libraries(mq_bandwidth ${AKBENCH_LIBS} rt)

add_library(mmap_bandwidth mmap_bandwidth.cc)
target_link_libraries(mmap_bandwidth ${AKBENCH_LIBS})

add_library(shm_bandwidth shm_bandwidth.cc)
target_link_libraries(shm_bandwidth ${AKBENCH_LIBS})

add_executable(barrier_test barrier_test.cc)
target_link_libraries(barrier_test ${AKBENCH_LIBS})
add_test(NAME barrier_test_constructor COMMAND barrier_test
                                               --test-type=constructor)
add_test(NAME barrier_test_wait_with_random_sleep
         COMMAND barrier_test --test-type=wait_with_random_sleep
                 --num-iterations=3)
add_test(NAME barrier_test_wait_without_sleep
         COMMAND barrier_test --test-type=wait_without_sleep --num-iterations=3)

add_executable(aklog_test aklog_test.cc)
target_link_libraries(aklog_test aklog)
add_test(NAME aklog_test COMMAND aklog_test)

# Bandwidth benchmark tests
add_executable(memcpy_bandwidth_test memcpy_bandwidth_test.cc)
target_link_libraries(memcpy_bandwidth_test memcpy_bandwidth ${AKBENCH_LIBS})
add_test(NAME memcpy_bandwidth_test COMMAND memcpy_bandwidth_test)

add_executable(memcpy_mt_bandwidth_test memcpy_mt_bandwidth_test.cc)
target_link_libraries(memcpy_mt_bandwidth_test memcpy_mt_bandwidth
                      ${AKBENCH_LIBS})
add_test(NAME memcpy_mt_bandwidth_test COMMAND memcpy_mt_bandwidth_test)

add_executable(tcp_bandwidth_test tcp_bandwidth_test.cc)
target_link_libraries(tcp_bandwidth_test tcp_bandwidth ${AKBENCH_LIBS})
add_test(NAME tcp_bandwidth_test COMMAND tcp_bandwidth_test)

add_executable(uds_bandwidth_test uds_bandwidth_test.cc)
target_link_libraries(uds_bandwidth_test uds_bandwidth ${AKBENCH_LIBS})
add_test(NAME uds_bandwidth_test COMMAND uds_bandwidth_test)

add_executable(pipe_bandwidth_test pipe_bandwidth_test.cc)
target_link_libraries(pipe_bandwidth_test pipe_bandwidth ${AKBENCH_LIBS})
add_test(NAME pipe_bandwidth_test COMMAND pipe_bandwidth_test)

add_executable(fifo_bandwidth_test fifo_bandwidth_test.cc)
target_link_libraries(fifo_bandwidth_test fifo_bandwidth ${AKBENCH_LIBS})
add_test(NAME fifo_bandwidth_test COMMAND fifo_bandwidth_test)

add_executable(mq_bandwidth_test mq_bandwidth_test.cc)
target_link_libraries(mq_bandwidth_test mq_bandwidth ${AKBENCH_LIBS})
add_test(NAME mq_bandwidth_test COMMAND mq_bandwidth_test)

add_executable(mmap_bandwidth_test mmap_bandwidth_test.cc)
target_link_libraries(mmap_bandwidth_test mmap_bandwidth ${AKBENCH_LIBS})
add_test(NAME mmap_bandwidth_test COMMAND mmap_bandwidth_test)

add_executable(shm_bandwidth_test shm_bandwidth_test.cc)
target_link_libraries(shm_bandwidth_test shm_bandwidth ${AKBENCH_LIBS})
add_test(NAME shm_bandwidth_test COMMAND shm_bandwidth_test)

# Latency benchmark tests
add_executable(atomic_latency_test atomic_latency_test.cc)
target_link_libraries(atomic_latency_test atomic_latency ${AKBENCH_LIBS})
add_test(NAME atomic_latency_test COMMAND atomic_latency_test)

add_executable(atomic_rel_acq_latency_test atomic_rel_acq_latency_test.cc)
target_link_libraries(atomic_rel_acq_latency_test atomic_rel_acq_latency
                      ${AKBENCH_LIBS})
add_test(NAME atomic_rel_acq_latency_test COMMAND atomic_rel_acq_latency_test)

add_executable(barrier_latency_test barrier_latency_test.cc)
target_link_libraries(barrier_latency_test barrier_latency ${AKBENCH_LIBS})
add_test(NAME barrier_latency_test COMMAND barrier_latency_test)

add_executable(condition_variable_latency_test
               condition_variable_latency_test.cc)
target_link_libraries(condition_variable_latency_test
                      condition_variable_latency ${AKBENCH_LIBS})
add_test(NAME condition_variable_latency_test
         COMMAND condition_variable_latency_test)

add_executable(semaphore_latency_test semaphore_latency_test.cc)
target_link_libraries(semaphore_latency_test semaphore_latency ${AKBENCH_LIBS})
add_test(NAME semaphore_latency_test COMMAND semaphore_latency_test)

add_executable(syscall_latency_test syscall_latency_test.cc)
target_link_libraries(syscall_latency_test syscall_latency ${AKBENCH_LIBS})
add_test(NAME syscall_latency_test COMMAND syscall_latency_test)

if(AKBENCH_ENABLE_MPI)
  find_package(MPI REQUIRED COMPONENTS CXX)
  add_executable(mpi_bandwidth mpi_bandwidth.cc)
  target_link_libraries(mpi_bandwidth PRIVATE MPI::MPI_CXX ${AKBENCH_LIBS})
  add_test(NAME mpi_bandwidth_test
           COMMAND mpirun -np 2 ${CMAKE_CURRENT_BINARY_DIR}/mpi_bandwidth
                   --data-size=1024)
endif()

add_library(atomic_latency atomic_latency.cc)
target_link_libraries(atomic_latency ${AKBENCH_LIBS})

add_library(atomic_rel_acq_latency atomic_rel_acq_latency.cc)
target_link_libraries(atomic_rel_acq_latency ${AKBENCH_LIBS})

add_library(barrier_latency barrier_latency.cc)
target_link_libraries(barrier_latency ${AKBENCH_LIBS})

add_library(condition_variable_latency condition_variable_latency.cc)
target_link_libraries(condition_variable_latency ${AKBENCH_LIBS})

add_library(semaphore_latency semaphore_latency.cc)
target_link_libraries(semaphore_latency ${AKBENCH_LIBS})

add_library(syscall_latency syscall_latency.cc)
target_link_libraries(syscall_latency ${AKBENCH_LIBS})

add_executable(akbench akbench.cc)
target_link_libraries(
  akbench
  # Latency libraries
  atomic_latency
  atomic_rel_acq_latency
  barrier_latency
  condition_variable_latency
  semaphore_latency
  syscall_latency
  # Bandwidth libraries
  memcpy_bandwidth
  memcpy_mt_bandwidth
  tcp_bandwidth
  uds_bandwidth
  pipe_bandwidth
  fifo_bandwidth
  mq_bandwidth
  mmap_bandwidth
  shm_bandwidth
  ${AKBENCH_LIBS})

# Test JSON output functionality
add_test(NAME akbench_json_test
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/akbench_json_test.sh
                 ${CMAKE_CURRENT_BINARY_DIR}/akbench)

install(TARGETS akbench DESTINATION ${CMAKE_INSTALL_BINDIR})
