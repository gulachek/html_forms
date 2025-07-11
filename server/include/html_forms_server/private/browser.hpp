/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef BROWSER_HPP
#define BROWSER_HPP

#include "html_forms_server.h"
#include "html_forms_server/private/asio-pch.hpp"

#include <msgstream.h>

#include <string>

class browser {
public:
  struct window_watcher {
    virtual ~window_watcher();
    virtual void window_close_requested() = 0;
  };

  browser();

  void run();
  void add_session(const std::string &session,
                   const std::weak_ptr<window_watcher> &watcher);
  void remove_session(const std::string &session);
  void show_error(const std::string &session, const std::string &msg);

  void load_url(const std::string &session, const std::string_view &url);

  void accept_io_transfer(const std::string &session,
                          const std::string_view &token);

  void set_event_callback(html_forms_server_event_callback *cb, void *ctx);

  void request_close(const std::string &session);

private:
  std::map<std::string, std::weak_ptr<window_watcher>> watchers_;

  void *event_ctx_;
  std::function<html_forms_server_event_callback> event_cb_;
  void notify_event(const html_forms_server_event &ev);
};

#endif
