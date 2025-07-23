#pragma once

#include <cstdint>
#include <semaphore.h>
#include <string>

class SenseReversingBarrier {
public:
  SenseReversingBarrier(int n, const std::string &id);
  ~SenseReversingBarrier();
  void Wait();
  static void ClearResource(const std::string &id);

private:
  struct ShmData {
    uint64_t count_{0};
    bool shared_sense_{false};
    uint64_t n_users_{0};
  };

  sem_t *shm_sem_;
  int shm_fd_;
  ShmData *shm_data_;
  bool sense_ = true;

  const uint64_t n_;
  const std::string init_sem_id_;
  const std::string shm_sem_id_;
  const std::string shm_id_;
};
