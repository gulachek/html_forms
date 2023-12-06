#include "browser.hpp"
#include "async_msgstream.hpp"

namespace bp = boost::process;
namespace asio = boost::asio;
namespace json = boost::json;

using load_url_handler = browser::load_url_handler;

#define BUF_SIZE 2048

browser::browser(asio::io_context &ioc)
    : exe_{bp::search_path(BROWSER_EXE)}, stdin_{ioc}, stdout_{ioc},
      next_window_id_{1} {
  argv_ = BROWSER_ARGS;
  in_buf_.resize(BUF_SIZE);
}

void browser::async_load_url(const std::string_view &url,
                             const std::function<load_url_handler> &cb) {
  proc();

  auto window = next_window_id_++;
  load_url_handlers_[window] = cb;

  json::object obj;
  obj["type"] = "open";
  obj["url"] = url;
  obj["windowId"] = window;
  out_buf_ = json::serialize(obj);
  if (out_buf_.size() > BUF_SIZE) {
    cb(std::make_error_condition(std::errc::no_buffer_space), -1);
    return;
  }

  async_msgstream_send(stdin_, asio::buffer(out_buf_.data(), BUF_SIZE),
                       out_buf_.size(),
                       std::bind_front(&browser::on_write_url, this, window));
}

void browser::on_write_url(window_id window, std::error_condition ec,
                           msgstream_size n) {
  auto node = load_url_handlers_.extract(window);
  if (node.empty())
    return;

  auto &cb = node.mapped();

  if (ec) {
    cb(ec, window);
    return;
  }

  cb(std::error_condition{}, window);
}

std::shared_ptr<bp::child> browser::proc() {
  if (!proc_) {
    std::cerr << exe_;
    for (auto arg : argv_) {
      std::cerr << ' ' << arg;
    }
    std::cerr << std::endl;

    proc_ = std::make_shared<bp::child>(
        exe_, argv_, bp::std_in<stdin_, bp::std_out> stdout_);
  }

  return proc_;
}
