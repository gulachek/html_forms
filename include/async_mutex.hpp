#ifndef ASYNC_MUTEX_HPP
#define ASYNC_MUTEX_HPP

#include "asio-pch.hpp"
#include "boost/asio/any_io_executor.hpp"
#include <mutex>
#include <queue>

template <typename Executor = boost::asio::any_io_executor> class async_mutex {
  typedef async_mutex<Executor> self;

public:
  typedef Executor executor_type;

  class lock {
    self &mtx_;

  public:
    lock(self &mtx) : mtx_{mtx} {}
    ~lock();

    lock(const lock &copy) = delete;
    lock &operator=(const lock &copy) = delete;

    lock(lock &&move) = default;
    lock &operator=(lock &&move) = default;
  };

  typedef std::shared_ptr<lock> lock_ptr;

  typedef void lock_handler(lock_ptr);

private:
  const executor_type &executor_;
  std::mutex mtx_;
  bool is_locked_;
  std::queue<std::function<lock_handler>> handlers_;

  lock_ptr mklock() { return std::make_shared<lock>(*this); }

  void unlock() {
    std::function<lock_handler> handler;

    {
      std::unique_lock lck{mtx_};
      // more handlers to run
      is_locked_ = handlers_.size() > 0;

      if (!is_locked_)
        return;

      handler = std::move(handlers_.front());
      handlers_.pop();
    }

    handler(mklock());
  }

public:
  template <typename Executor1> struct rebind_executor {
    typedef async_mutex<Executor1> other;
  };

  async_mutex(const executor_type &ex) : executor_{ex}, is_locked_{false} {}

  const executor_type &get_executor() { return executor_; }

  template <boost::asio::completion_token_for<lock_handler> CompletionToken>
  auto async_lock(CompletionToken &&token) {
    auto init = [this](auto completion_handler) {
      bool run_now = false;

      {
        std::unique_lock lck{mtx_};
        if (is_locked_) {
          handlers_.emplace(std::move(completion_handler));
        } else {
          is_locked_ = true;
          run_now = true;
        }
      }

      if (run_now)
        completion_handler(mklock());
    };

    return boost::asio::async_initiate<CompletionToken, lock_handler>(init,
                                                                      token);
  }
};

template <typename Executor> async_mutex<Executor>::lock::~lock() {
  mtx_.unlock();
}

#endif
