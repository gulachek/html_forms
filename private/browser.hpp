#ifndef BROWSER_HPP
#define BROWSER_HPP

#include "asio-pch.hpp"
#include "html_forms/server.h"

#include <msgstream.h>

class browser {
public:
  using window_id = int;

  struct window_watcher {
    virtual ~window_watcher();
    virtual void window_close_requested() = 0;
  };

  browser();

  void run();
  window_id reserve_window(const std::weak_ptr<window_watcher> &watcher);
  void release_window(window_id window);
  void show_error(window_id window, const std::string &msg);

  void load_url(window_id window, const std::string_view &url);

  void set_event_callback(html_forms_server_event_callback *cb, void *ctx);

  void request_close(window_id window);

private:
  std::atomic<window_id> next_window_id_;
  std::map<window_id, std::weak_ptr<window_watcher>> watchers_;

  void *event_ctx_;
  std::function<html_forms_server_event_callback> event_cb_;
  void notify_event(const html_forms_server_event &ev);
};

#endif
