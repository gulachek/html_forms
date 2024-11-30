#include "browser.hpp"
#include "html_forms/server.h"
#include <iterator>

using close_window_handler = browser::close_window_handler;
using window_id = browser::window_id;
using window_watcher = browser::window_watcher;

browser::browser() : next_window_id_{1} {}

void browser::request_close(window_id window) {
  auto it = watchers_.find(window);
  if (it == watchers_.end()) {
    std::cerr << "Attempting to close window that has no watcher: " << window
              << std::endl;
    return;
  }

  if (auto win_ptr = it->second.lock()) {
    win_ptr->window_close_requested();
  } else {
    watchers_.erase(it);
  }
}

window_id
browser::reserve_window(const std::weak_ptr<window_watcher> &watcher) {
  auto win = next_window_id_++;
  watchers_[win] = watcher;
  return win;
}

void browser::release_window(window_id window) {
  watchers_.erase(window);
  async_close_window(window, [](std::error_condition) {});
}

void browser::show_error(window_id window, const std::string &msg) {
  html_forms_server_event ev;
  ev.type = HTML_FORMS_SERVER_EVENT_SHOW_ERROR;
  ev.data.show_err.window_id = window;
  ::strlcpy(ev.data.show_err.msg, msg.data(), sizeof(ev.data.show_err.msg));
  notify_event(ev);
}

void browser::load_url(window_id window, const std::string_view &url) {

  html_forms_server_event ev;
  ev.type = HTML_FORMS_SERVER_EVENT_OPEN_URL;
  ev.data.open_url.window_id = window;
  ::strlcpy(ev.data.open_url.url, url.data(), sizeof(ev.data.open_url.url));
  notify_event(ev);
}

void browser::async_close_window(
    window_id window, const std::function<close_window_handler> &cb) {
  html_forms_server_event ev;
  ev.type = HTML_FORMS_SERVER_EVENT_CLOSE_WINDOW;
  ev.data.close_win.window_id = window;
  notify_event(ev);
  cb(std::error_condition{});
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
