/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef SESSION_LOCK_HPP
#define SESSION_LOCK_HPP

#include <memory>
#include <string>

class session_lock {
public:
  session_lock(const std::string &path = "");
  ~session_lock();

  operator bool() const;
  bool is_open() const;

  bool open(const std::string &path);
  bool try_lock();
  void unlock();

private:
  struct impl;
  std::unique_ptr<impl> pimpl_;
};

#endif
