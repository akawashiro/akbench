#include "barrier.h"

#include <fcntl.h>
#include <sys/mman.h>

#include <thread>

#include "absl/log/check.h"
#include "absl/log/log.h"

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
  CHECK(shm_sem_ != SEM_FAILED) << "Failed to create semaphore with id '"
                                << shm_sem_id_ << "': " << strerror(errno);

  // Critical section to ensure all processes hold initialized shared memory.
  sem_wait(shm_sem_);
  shm_fd_ = shm_open(shm_id_.c_str(), O_CREAT | O_RDWR | O_EXCL, 0644);
  if (shm_fd_ >= 0) {
    VLOG(2) << "Created shared memory with id '" << shm_id_ << "'";
    CHECK(ftruncate(shm_fd_, sizeof(ShmData)) == 0)
        << "Failed to set size of shared memory with id '" << shm_id_
        << "': " << strerror(errno);
    shm_data_ = static_cast<ShmData *>(mmap(nullptr, sizeof(ShmData),
                                            PROT_READ | PROT_WRITE, MAP_SHARED,
                                            shm_fd_, 0));
    CHECK(shm_data_ != MAP_FAILED) << "Failed to map shared memory with id '"
                                   << shm_id_ << "': " << strerror(errno);

    shm_data_->count_ = 0;
    shm_data_->shared_sense_ = false;
    shm_data_->n_users_ = 0;
  } else if (shm_fd_ < 0) {
    if (errno == EEXIST) {
      VLOG(2) << "Shared memory with id '" << shm_id_
              << "' already exists, opening it instead.";
      shm_fd_ = shm_open(shm_id_.c_str(), O_RDWR, 0644);
      CHECK(shm_fd_ >= 0) << "Failed to open existing shared memory with id '"
                          << shm_id_ << "': " << strerror(errno);
      shm_data_ = static_cast<ShmData *>(mmap(nullptr, sizeof(ShmData),
                                              PROT_READ | PROT_WRITE,
                                              MAP_SHARED, shm_fd_, 0));
      CHECK(shm_data_ != MAP_FAILED)
          << "Failed to map existing shared memory with id '" << shm_id_
          << "': " << strerror(errno);
    } else {
      CHECK(false) << "Failed to create shared memory with id '" << shm_id_
                   << "': " << strerror(errno);
    }
  }
  sem_post(shm_sem_);
  // Critical section ends here.

  sem_wait(shm_sem_);
  shm_data_->n_users_++;
  sem_post(shm_sem_);

  VLOG(2) << "SenseReversingBarrier initialized with id '" << shm_id_
          << "' for " << n_ << " users. Waiting for all users to join.";
  while (true) {
    sem_wait(shm_sem_);
    bool all_users_joined = shm_data_->n_users_ >= n_;
    sem_post(shm_sem_);
    if (all_users_joined) {
      break;
    }
    std::this_thread::yield();
  }
  VLOG(2) << "All users have joined the barrier with id '" << shm_id_
          << "'. Proceeding.";
}

void SenseReversingBarrier::Wait() {
  bool last_user = false;
  sem_wait(shm_sem_);
  shm_data_->count_++;
  if (shm_data_->count_ == n_) {
    last_user = true;
    shm_data_->shared_sense_ = !shm_data_->shared_sense_;
    shm_data_->count_ = 0;
    VLOG(2) << "All users reached the barrier with id '" << shm_id_
            << "'. Sense reversed to " << shm_data_->shared_sense_;
  }
  sem_post(shm_sem_);

  if (!last_user) {
    VLOG(2) << "Waiting for other users to reach the barrier with id '"
            << shm_id_ << "'.";
    while (true) {
      sem_wait(shm_sem_);
      bool reversed = shm_data_->shared_sense_ != sense_;
      sem_post(shm_sem_);
      if (!reversed) {
        break;
      }
    }
    VLOG(2) << "All users reached the barrier with id '" << shm_id_;
  }

  sense_ = !sense_;
}

SenseReversingBarrier::~SenseReversingBarrier() {
  sem_wait(shm_sem_);
  uint64_t remaining_users = shm_data_->n_users_;
  shm_data_->n_users_--;
  sem_post(shm_sem_);
  if (remaining_users == 1) {
    VLOG(2) << "Last user of shared memory with id '" << shm_id_
            << "' is exiting. Unlinking shared memory.";
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
    VLOG(2) << "Not the last user of shared memory with id '" << shm_id_ << " "
            << remaining_users << " users remaining.";
    if (shm_sem_) {
      sem_close(shm_sem_);
    }
    if (shm_fd_ >= 0) {
      munmap(shm_data_, sizeof(ShmData));
      close(shm_fd_);
    }
  }
}
