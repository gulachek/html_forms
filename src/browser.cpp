#include "browser.hpp"
#include "async_msgstream.hpp"
#include "boost/asio/any_io_executor.hpp"
#include <iterator>

namespace bp = boost::process;
namespace asio = boost::asio;
namespace json = boost::json;

using load_url_handler = browser::load_url_handler;
using close_window_handler = browser::close_window_handler;
using lock_ptr = async_mutex<asio::any_io_executor>::lock_ptr;
using window_id = browser::window_id;
using window_watcher = browser::window_watcher;

#define BUF_SIZE 2048

browser::browser(asio::io_context &ioc)
    : exe_{bp::search_path(BROWSER_EXE)}, stdin_{ioc}, stdout_{ioc},
      next_window_id_{1}, mtx_{ioc.get_executor()} {
  argv_ = BROWSER_ARGS;
  in_buf_.resize(BUF_SIZE);
}

void browser::run() {
  proc_ = std::make_shared<bp::child>(exe_, argv_,
                                      bp::std_in<stdin_, bp::std_out> stdout_);

  asio::post(stdout_.get_executor(), bind(&browser::do_recv));
}

void browser::do_recv() {
  std::cerr << "Trying to receive browser message" << std::endl;
  async_msgstream_recv(stdout_, asio::buffer(in_buf_), bind(&browser::on_recv));
  /*
stdout_.async_read_some(
asio::buffer(in_buf_), [this](std::error_code ec, std::size_t n) {
  if (ec) {
    std::cerr << "Browser failed to read message: " << ec.message()
              << std::endl;
    return;
  }

  std::string_view msg{(char *)in_buf_.data(), n};
  std::cerr << "read some bytes from browser: " << msg << std::endl;
  do_recv();
});
                  */
}

void browser::on_recv(std::error_condition ec, std::size_t n) {
  if (ec) {
    std::cerr << "Browser failed to read message: " << ec.message()
              << std::endl;
    // TODO - need to crash or something. this isn't good
    return;
  }

  std::string_view msg{(char *)in_buf_.data(), n};
  std::cerr << "Received browser message: " << msg << std::endl;

  auto obj = json::parse(msg).as_object();
  auto type = obj["type"].as_string();
  if (type == "close") {
    int window = obj["windowId"].as_int64();
    auto it = watchers_.find(window);
    if (it == watchers_.end()) {
      std::cerr << "Attempting to close window that has no watcher: " << window
                << std::endl;
      return;
    }

    if (auto win_ptr = it->second.lock())
      win_ptr->window_close_requested();
  } else {
    std::cerr << "Unexpected message from browser: " << type << std::endl;
    return;
  }

  do_recv();
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

void browser::send_msg(
    const json::object &obj,
    const std::function<void(std::error_condition, std::size_t)> &cb) {
  out_buf_ = json::serialize(obj);
  if (out_buf_.size() > BUF_SIZE) {
    cb(std::make_error_condition(std::errc::no_buffer_space), -1);
    return;
  }

  async_msgstream_send(stdin_, asio::buffer(out_buf_.data(), BUF_SIZE),
                       out_buf_.size(), cb);
}

void browser::async_load_url(window_id window, const std::string_view &url,
                             const std::function<load_url_handler> &cb) {

  load_url_handlers_[window] = cb;

  mtx_.async_lock([this, window, url = std::string{url}](lock_ptr lock) {
    json::object obj;
    obj["type"] = "open";
    obj["url"] = url;
    obj["windowId"] = window;

    send_msg(obj, bind(&browser::on_write_url, window, lock));
  });
}

void browser::on_write_url(window_id window, lock_ptr lock,
                           std::error_condition ec, std::size_t n) {
  auto node = load_url_handlers_.extract(window);
  if (node.empty())
    return;

  auto &cb = node.mapped();
  cb(ec);
}

void browser::async_close_window(
    window_id window, const std::function<close_window_handler> &cb) {
  if (!proc_)
    cb(std::error_condition{});

  mtx_.async_lock([this, cb, window](lock_ptr lock) {
    json::object obj;
    obj["type"] = "close";
    obj["windowId"] = window;

    send_msg(obj, [cb](std::error_condition ec, std::size_t n) { cb(ec); });
  });
}

browser::window_watcher::~window_watcher() {}
