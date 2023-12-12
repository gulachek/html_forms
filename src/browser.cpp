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

#define BUF_SIZE 2048

browser::browser(asio::io_context &ioc)
    : exe_{bp::search_path(BROWSER_EXE)}, stdin_{ioc}, stdout_{ioc},
      next_window_id_{1}, mtx_{ioc.get_executor()} {
  argv_ = BROWSER_ARGS;
  in_buf_.resize(BUF_SIZE);
}

void browser::send_msg(
    const json::object &obj,
    const std::function<void(std::error_condition, msgstream_size)> &cb) {
  out_buf_ = json::serialize(obj);
  if (out_buf_.size() > BUF_SIZE) {
    cb(std::make_error_condition(std::errc::no_buffer_space), -1);
    return;
  }

  async_msgstream_send(stdin_, asio::buffer(out_buf_.data(), BUF_SIZE),
                       out_buf_.size(), cb);
}

window_id browser::async_load_url(const std::string_view &url,
                                  const std::optional<window_id> &window,
                                  const std::function<load_url_handler> &cb) {
  proc();

  window_id win;
  if (window) {
    win = *window;
  } else {
    win = next_window_id_++;
  }

  mtx_.async_lock([this, cb, win, url = std::string{url}](lock_ptr lock) {
    load_url_handlers_[win] = cb;

    json::object obj;
    obj["type"] = "open";
    obj["url"] = url;
    obj["windowId"] = win;

    send_msg(obj, bind(&browser::on_write_url, win, lock));
  });

  return win;
}

void browser::on_write_url(window_id window, lock_ptr lock,
                           std::error_condition ec, msgstream_size n) {
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

    send_msg(obj, [cb](std::error_condition ec, msgstream_size n) { cb(ec); });
  });
}

std::shared_ptr<bp::child> browser::proc() {
  if (!proc_) {
    proc_ = std::make_shared<bp::child>(
        exe_, argv_, bp::std_in<stdin_, bp::std_out> stdout_);
  }

  return proc_;
}
