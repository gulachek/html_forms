#include "session_lock.hpp"

#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>

struct session_lock::impl {
  sem_t *sem;
  std::string name;
};

session_lock::session_lock(const std::string &name) {
  pimpl_ = std::make_unique<impl>();
  pimpl_->sem = SEM_FAILED;

  if (!name.empty())
    open(name);
}

session_lock::~session_lock() {
  if (is_open()) {
    ::sem_close(pimpl_->sem);
    ::sem_unlink(pimpl_->name.c_str());
  }
}

bool session_lock::is_open() const { return pimpl_->sem != SEM_FAILED; }
session_lock::operator bool() const { return is_open(); }

bool session_lock::open(const std::string &name) {
  if (is_open())
    return false;

  pimpl_->name = name.size() > 32 ? name.substr(32) : name;
  pimpl_->sem = ::sem_open(pimpl_->name.c_str(), O_CREAT, S_IRUSR | S_IWUSR, 1);
  return is_open();
}

bool session_lock::try_lock() {
  auto sem = pimpl_->sem;

  if (::sem_trywait(sem) == -1) {
    return false;
  }

  return true;
}

void session_lock::unlock() {
  auto sem = pimpl_->sem;
  ::sem_post(sem);
}
