#include "barrier.h"

#include <fcntl.h>
#include <sys/mman.h>

#include <cstring>
#include <format>
#include <thread>

#include "aklog.h"

void SenseReversingBarrier::ClearResource(const std::string &id) {
  const std::string init_sem_id = id + "_init_sem";
  const std::string shm_sem_id = id + "_shm_sem";
  const std::string shm_id = id + "_shm";
  sem_unlink(init_sem_id.c_str());
  sem_unlink(shm_sem_id.c_str());
  shm_unlink(shm_id.c_str());
}

SenseReversingBarrier::SenseReversingBarrier(int n, const std::string &id)
    : n_(n), init_sem_id_(id + "_init_sem"), shm_sem_id_(id + "_shm_sem"),
      shm_id_(id + "_shm") {
  shm_sem_ = sem_open(shm_sem_id_.c_str(), O_CREAT, 0644, 1);
  AKCHECK(shm_sem_ != SEM_FAILED,
          std::format("Failed to create semaphore with id '{}': {}",
                      shm_sem_id_, strerror(errno)));

  // Critical section to ensure all processes hold initialized shared memory.
  sem_wait(shm_sem_);
  shm_fd_ = shm_open(shm_id_.c_str(), O_CREAT | O_RDWR | O_EXCL, 0644);
  if (shm_fd_ >= 0) {
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("Created shared memory with id '{}'", shm_id_));
    AKCHECK(ftruncate(shm_fd_, sizeof(ShmData)) == 0,
            std::format("Failed to set size of shared memory with id '{}': {}",
                        shm_id_, strerror(errno)));
    shm_data_ = static_cast<ShmData *>(mmap(nullptr, sizeof(ShmData),
                                            PROT_READ | PROT_WRITE, MAP_SHARED,
                                            shm_fd_, 0));
    AKCHECK(shm_data_ != MAP_FAILED,
            std::format("Failed to map shared memory with id '{}': {}", shm_id_,
                        strerror(errno)));

    shm_data_->count_ = 0;
    shm_data_->shared_sense_ = false;
    shm_data_->n_users_ = 0;
  } else if (shm_fd_ < 0) {
    if (errno == EEXIST) {
      AKLOG(
          aklog::LogLevel::DEBUG,
          std::format(
              "Shared memory with id '{}' already exists, opening it instead.",
              shm_id_));
      shm_fd_ = shm_open(shm_id_.c_str(), O_RDWR, 0644);
      AKCHECK(
          shm_fd_ >= 0,
          std::format("Failed to open existing shared memory with id '{}': {}",
                      shm_id_, strerror(errno)));
      shm_data_ = static_cast<ShmData *>(mmap(nullptr, sizeof(ShmData),
                                              PROT_READ | PROT_WRITE,
                                              MAP_SHARED, shm_fd_, 0));
      AKCHECK(
          shm_data_ != MAP_FAILED,
          std::format("Failed to map existing shared memory with id '{}': {}",
                      shm_id_, strerror(errno)));
    } else {
      AKCHECK(false,
              std::format("Failed to create shared memory with id '{}': {}",
                          shm_id_, strerror(errno)));
    }
  }
  sem_post(shm_sem_);
  // Critical section ends here.

  sem_wait(shm_sem_);
  shm_data_->n_users_++;
  sem_post(shm_sem_);

  AKLOG(aklog::LogLevel::DEBUG,
        std::format("SenseReversingBarrier initialized with id '{}' for {} "
                    "users. Waiting for all users to join.",
                    shm_id_, n_));
  while (true) {
    sem_wait(shm_sem_);
    bool all_users_joined = shm_data_->n_users_ >= n_;
    sem_post(shm_sem_);
    if (all_users_joined) {
      break;
    }
    std::this_thread::yield();
  }
  AKLOG(
      aklog::LogLevel::DEBUG,
      std::format("All users have joined the barrier with id '{}'. Proceeding.",
                  shm_id_));
}

void SenseReversingBarrier::Wait() {
  bool last_user = false;
  sem_wait(shm_sem_);
  shm_data_->count_++;
  if (shm_data_->count_ == n_) {
    last_user = true;
    shm_data_->shared_sense_ = !shm_data_->shared_sense_;
    shm_data_->count_ = 0;
    AKLOG(
        aklog::LogLevel::DEBUG,
        std::format(
            "All users reached the barrier with id '{}'. Sense reversed to {}",
            shm_id_, shm_data_->shared_sense_));
  }
  sem_post(shm_sem_);

  if (!last_user) {
    AKLOG(aklog::LogLevel::DEBUG,
          std::format(
              "Waiting for other users to reach the barrier with id '{}'.",
              shm_id_));
    while (true) {
      sem_wait(shm_sem_);
      bool reversed = shm_data_->shared_sense_ != sense_;
      sem_post(shm_sem_);
      if (!reversed) {
        break;
      }
    }
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("All users reached the barrier with id '{}'", shm_id_));
  }

  sense_ = !sense_;
}

SenseReversingBarrier::~SenseReversingBarrier() {
  sem_wait(shm_sem_);
  uint64_t remaining_users = shm_data_->n_users_;
  shm_data_->n_users_--;
  sem_post(shm_sem_);
  if (remaining_users == 1) {
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("Last user of shared memory with id '{}' is exiting. "
                      "Unlinking shared memory.",
                      shm_id_));
    if (shm_sem_) {
      sem_close(shm_sem_);
      sem_unlink(shm_sem_id_.c_str());
    }
    if (shm_fd_ >= 0) {
      munmap(shm_data_, sizeof(ShmData));
      close(shm_fd_);
      shm_unlink(shm_id_.c_str());
    }
  } else {
    AKLOG(aklog::LogLevel::DEBUG,
          std::format("Not the last user of shared memory with id '{}' {} "
                      "users remaining.",
                      shm_id_, remaining_users));
    if (shm_sem_) {
      sem_close(shm_sem_);
    }
    if (shm_fd_ >= 0) {
      munmap(shm_data_, sizeof(ShmData));
      close(shm_fd_);
    }
  }
}
