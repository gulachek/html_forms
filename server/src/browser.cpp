/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#include "html_forms_server/private/browser.hpp"
#include "html_forms_server.h"
#include "html_forms_server/private/evt_util.hpp"
#include <iterator>

using window_watcher = browser::window_watcher;

browser::browser() {}

void browser::request_close(const std::string &session) {
  auto it = watchers_.find(session);
  if (it == watchers_.end()) {
    std::cerr << "Attempting to close session that has no watcher: " << session
              << std::endl;
    return;
  }

  if (auto win_ptr = it->second.lock()) {
    win_ptr->window_close_requested();
  } else {
    watchers_.erase(it);
  }
}

void browser::add_session(const std::string &session,
                          const std::weak_ptr<window_watcher> &watcher) {
  watchers_[session] = watcher;
}

void browser::remove_session(const std::string &session) {
  watchers_.erase(session);
  html_forms_server_event ev;
  ev.type = HTML_FORMS_SERVER_EVENT_CLOSE_WINDOW;
  copy_session_id(session, ev.data.close_win.session_id);
  notify_event(ev);
}

void browser::show_error(const std::string &session, const std::string &msg) {
  html_forms_server_event ev;
  ev.type = HTML_FORMS_SERVER_EVENT_SHOW_ERROR;
  copy_session_id(session, ev.data.show_err.session_id);
  ::strlcpy(ev.data.show_err.msg, msg.data(), sizeof(ev.data.show_err.msg));
  notify_event(ev);
}

void browser::load_url(const std::string &session,
                       const std::string_view &url) {

  html_forms_server_event ev;
  ev.type = HTML_FORMS_SERVER_EVENT_OPEN_URL;
  copy_session_id(session, ev.data.open_url.session_id);
  ::strlcpy(ev.data.open_url.url, url.data(), sizeof(ev.data.open_url.url));
  notify_event(ev);
}

void browser::accept_io_transfer(const std::string &session,
                                 const std::string_view &token) {
  html_forms_server_event ev;
  ev.type = HTML_FORMS_SERVER_EVENT_ACCEPT_IO_TRANSFER;
  copy_session_id(session, ev.data.accept_io_transfer.session_id);
  ::strlcpy(ev.data.accept_io_transfer.token, token.data(),
            sizeof(ev.data.accept_io_transfer.token));
  notify_event(ev);
}

browser::window_watcher::~window_watcher() {}

void browser::set_event_callback(html_forms_server_event_callback *cb,
                                 void *ctx) {
  event_cb_ = cb;
  event_ctx_ = ctx;
}

void browser::notify_event(const html_forms_server_event &ev) {
  if (event_cb_) {
    event_cb_(&ev, event_ctx_);
  }
}
