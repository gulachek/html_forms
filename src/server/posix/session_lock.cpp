#include "session_lock.hpp"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

struct session_lock::impl {
  int fd = -1;
  std::string path;
};

session_lock::session_lock(const std::string &path) {
  pimpl_ = std::make_unique<impl>();

  if (!path.empty())
    open(path);
}

session_lock::~session_lock() {
  if (is_open())
    ::close(pimpl_->fd);
}

bool session_lock::is_open() const { return pimpl_->fd != -1; }
session_lock::operator bool() const { return is_open(); }

bool session_lock::open(const std::string &path) {
  if (is_open())
    return false;

  pimpl_->path = path;
  int fd = pimpl_->fd = ::open(path.c_str(), O_SEARCH);
  return is_open();
}

bool session_lock::try_lock() {
  return ::flock(pimpl_->fd, LOCK_EX | LOCK_NB) != -1;
}

void session_lock::unlock() { ::flock(pimpl_->fd, LOCK_UN); }
