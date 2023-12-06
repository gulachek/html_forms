#ifndef BROWSER_HPP
#define BROWSER_HPP

#include "asio-pch.hpp"
#include <msgstream.h>

class browser {
public:
  using window_id = int;
  using load_url_handler = void(std::error_condition, window_id);

  browser(boost::asio::io_context &ioc);

  void async_load_url(const std::string_view &url,
                      const std::function<load_url_handler> &cb);

  void async_close_window(window_id id,
                          const std::function<void(std::error_condition)> &cb);

private:
  std::shared_ptr<boost::process::child> proc();

  void on_write_url(window_id window, std::error_condition ec,
                    msgstream_size n);

  std::shared_ptr<boost::process::child> proc_;
  boost::process::filesystem::path exe_;
  std::vector<std::string> argv_;

  boost::process::async_pipe stdin_;
  boost::process::async_pipe stdout_;

  std::string out_buf_;
  std::vector<char> in_buf_;

  window_id next_window_id_;
  std::map<window_id, std::function<load_url_handler>> load_url_handlers_;
};

#endif
