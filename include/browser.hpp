#ifndef BROWSER_HPP
#define BROWSER_HPP

#include "asio-pch.hpp"
#include "async_mutex.hpp"

#include <msgstream.h>

class browser {
public:
  using window_id = int;
  using load_url_handler = void(std::error_condition);
  using close_window_handler = void(std::error_condition);
  using lock_ptr = async_mutex<boost::asio::any_io_executor>::lock_ptr;

  browser(boost::asio::io_context &ioc);

  window_id async_load_url(const std::string_view &url,
                           const std::optional<window_id> &window,
                           const std::function<load_url_handler> &cb);

  void async_close_window(window_id window,
                          const std::function<close_window_handler> &cb);

private:
  template <typename Fn, typename... Args> auto bind(Fn &&fn, Args &&...args) {
    return std::bind_front(std::forward<Fn>(fn), this,
                           std::forward<Args>(args)...);
  }

  std::shared_ptr<boost::process::child> proc();

  void on_write_url(window_id window, lock_ptr lock, std::error_condition ec,
                    msgstream_size n);

  void
  send_msg(const boost::json::object &obj,
           const std::function<void(std::error_condition, msgstream_size)> &cb);

  async_mutex<boost::asio::any_io_executor> mtx_;

  std::shared_ptr<boost::process::child> proc_;
  boost::process::filesystem::path exe_;
  std::vector<std::string> argv_;

  boost::process::async_pipe stdin_;
  boost::process::async_pipe stdout_;

  std::string out_buf_;
  std::vector<char> in_buf_;

  std::atomic<window_id> next_window_id_;
  std::map<window_id, std::function<load_url_handler>> load_url_handlers_;
};

#endif
