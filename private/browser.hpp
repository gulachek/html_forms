#ifndef BROWSER_HPP
#define BROWSER_HPP

#include "asio-pch.hpp"
#include "async_mutex.hpp"
#include "html_forms/server.h"
#include <boost/process.hpp>

#include <msgstream.h>

class browser {
public:
  using window_id = int;

  struct window_watcher {
    virtual ~window_watcher();
    virtual void window_close_requested() = 0;
  };

  using load_url_handler = void(std::error_condition);
  using close_window_handler = void(std::error_condition);
  using lock_ptr = async_mutex<boost::asio::any_io_executor>::lock_ptr;

  browser(boost::asio::io_context &ioc);

  void run();
  window_id reserve_window(const std::weak_ptr<window_watcher> &watcher);
  void release_window(window_id window);
  void show_error(window_id window, const std::string &msg);

  void async_load_url(window_id window, const std::string_view &url,
                      const std::function<load_url_handler> &cb);

  void async_close_window(window_id window,
                          const std::function<close_window_handler> &cb);

  void set_event_callback(html_forms_server_event_callback *cb, void *ctx);

  void request_close(window_id window);

private:
  template <typename Fn, typename... Args> auto bind(Fn &&fn, Args &&...args) {
    return std::bind_front(std::forward<Fn>(fn), this,
                           std::forward<Args>(args)...);
  }

  void do_recv();
  void on_recv(std::error_condition ec, std::size_t n);

  void on_write_url(window_id window, lock_ptr lock, std::error_condition ec,
                    std::size_t n);

  void
  send_msg(const boost::json::object &obj,
           const std::function<void(std::error_condition, std::size_t)> &cb);

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
  std::map<window_id, std::weak_ptr<window_watcher>> watchers_;

  void *event_ctx_;
  std::function<html_forms_server_event_callback> event_cb_;
  void notify_event(const html_forms_server_event &ev);
};

#endif
