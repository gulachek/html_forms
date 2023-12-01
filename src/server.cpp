//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: HTTP server, asynchronous
//
//------------------------------------------------------------------------------
#include "asio-pch.hpp"
#include "async_msgstream.hpp"
#include "catui.h"
#include "html-forms.h"
#include "http_listener.hpp"
#include "open-url.hpp"

extern "C" {
#include <catui_server.h>
}

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace json = boost::json;

using asio::ip::tcp;

typedef asio::posix::stream_descriptor catui_stream;
typedef ws::stream<beast::tcp_stream> ws_stream;

struct upload_file {
  std::vector<std::uint8_t> contents;
  std::string mime_type;
};

class catui_connection : public std::enable_shared_from_this<catui_connection>,
                         public http_session {
  using self = catui_connection;

  catui_stream stream_;
  std::vector<std::uint8_t> buf_;
  std::map<std::string, upload_file> uploads_;
  std::shared_ptr<http_listener> http_;
  std::string session_id_;
  std::string submit_buf_;
  beast::flat_buffer ws_buf_;
  std::string ws_send_buf_;

  std::shared_ptr<ws_stream> ws_;

  template <typename... FnArgs>
  auto bind(void (catui_connection::*fn)(FnArgs...)) {
    return std::bind_front(fn, shared_from_this());
  }

public:
  catui_connection(catui_stream &&stream,
                   const std::shared_ptr<http_listener> &http)
      : stream_{std::move(stream)}, http_{http} {}

  ~catui_connection() { http_->remove_session(session_id_); }

  // Start the asynchronous operation
  void run() {
    // TODO - thread safety on http_ ptr
    session_id_ = http_->add_session(weak_from_this());
    asio::dispatch(stream_.get_executor(), bind(&self::do_ack));
  }

  string_response respond(const std::string_view &target,
                          string_request &&req) override {

    switch (req.method()) {
    case http::verb::post:
      return respond_post(target, std::move(req));
    case http::verb::head:
    case http::verb::get:
      return respond_get(target, std::move(req));
    default:
      break;
    }

    string_response res{http::status::bad_request, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.keep_alive(req.keep_alive());

    std::ostringstream os;
    os << "Request method '" << req.method_string() << "' not supported";

    res.set(http::field::content_type, "text/plain");
    res.body() = os.str();
    res.prepare_payload();
    return res;
  }

  void connect_ws(boost::asio::ip::tcp::socket &&sock,
                  string_request &&req) override {
    if (ws_) {
      std::cerr << "Aborting websocket connection because one already exists "
                   "for the session"
                << std::endl;
      sock.close();
      return;
    }

    ws_ = std::make_shared<ws_stream>(std::move(sock));
    ws_->set_option(
        ws::stream_base::timeout::suggested(beast::role_type::server));

    ws_->async_accept(req, bind(&self::on_ws_accept));
  }

private:
  void do_ack() {
    buf_.resize(CATUI_ACK_SIZE);
    auto n = catui_server_encode_ack(buf_.data(), buf_.size(), stderr);
    if (n < 0)
      return;

    async_msgstream_send(stream_, asio::buffer(buf_), n, bind(&self::on_ack));
  }

  void on_ack(std::error_condition ec, msgstream_size n) {
    if (ec) {
      std::cerr << "Error sending ack: " << ec.message() << std::endl;
      return;
    }

    buf_.resize(HTML_MSG_SIZE);
    do_recv();
  }

  void do_recv() {
    async_msgstream_recv(stream_, asio::buffer(buf_), bind(&self::on_recv));
  }

  void on_recv(std::error_condition ec, msgstream_size n) {
    if (ec) {
      std::cerr << "Error receiving html message: " << ec.message()
                << std::endl;
      return;
    }

    struct html_msg msg;
    if (!html_decode_msg(buf_.data(), n, &msg)) {
      std::cerr << "Error decoding html message" << std::endl;
      return;
    }

    switch (msg.type) {
    case HTML_BEGIN_UPLOAD:
      do_read_upload(msg.msg.upload);
      break;
    case HTML_PROMPT:
      do_prompt(msg.msg.prompt);
      break;
    default:
      std::cerr << "Invalid message type: " << msg.type << std::endl;
      break;
    }
  }

  void on_ws_accept(beast::error_code ec) {
    if (ec) {
      std::cerr << "Failed to accept websocket: " << ec.message() << std::endl;
      return;
    }

    std::cerr << "Websocket connected" << std::endl;
    do_ws_read();
  }

  void do_ws_read() {
    if (!ws_) {
      std::cerr << "Invalid do_ws_read with no websocket connection"
                << std::endl;
      return;
    }

    ws_->async_read(ws_buf_, bind(&self::on_ws_read));
  }

  void on_ws_read(beast::error_code ec, std::size_t size) {
    if (ec) {
      std::cerr << "Failed to read ws message for session " << session_id_
                << std::endl;
      return;
    }

    auto msg = beast::buffers_to_string(ws_buf_.data());
    ws_buf_.consume(size);

    std::cerr << "Received message: " << msg << std::endl;
    do_ws_read();
  }

  void on_ws_write(beast::error_code ec, std::size_t size) {
    if (ec) {
      std::cerr << "Failed to send ws message for session " << session_id_
                << std::endl;
      return;
    }
  }

  string_response respond_post(const std::string_view &target,
                               string_request &&req) {
    if (target == "/submit") {
      std::cerr << "Initiating post with body: " << req.body() << std::endl;
      submit_buf_ = std::move(req.body());
      asio::dispatch(bind(&self::submit_post));

      string_response res{http::status::ok, req.version()};
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.keep_alive(req.keep_alive());
      res.set(http::field::content_type, "text/plain");
      res.content_length(2);
      res.body() = "ok";
      res.prepare_payload();
      return res;
    } else {
      return respond404(std::move(req));
    }
  }

  void submit_post() {
    std::cerr << "Posting body: " << submit_buf_ << std::endl;
    auto buf = asio::buffer(submit_buf_.data(), 4096);
    async_msgstream_send(stream_, buf, submit_buf_.size(),
                         bind(&self::on_submit_post));
  }

  void on_submit_post(std::error_condition ec, msgstream_size n) {
    if (ec) {
      std::cerr << "Error sending post to app: " << ec.message() << std::endl;
      return;
    }
  }

  string_response respond_get(const std::string_view &target,
                              string_request &&req) {
    auto upload_it = uploads_.find(std::string{target});
    if (upload_it == uploads_.end())
      return respond404(std::move(req));

    string_response res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.keep_alive(req.keep_alive());

    const auto &upload = upload_it->second;

    res.set(http::field::content_type, upload.mime_type);
    res.content_length(upload.contents.size());

    // Respond to HEAD request
    if (req.method() == http::verb::head) {
      res.body() = "";
      return res;
    }

    if (req.method() == http::verb::get) {
      res.body() = std::string{upload.contents.begin(), upload.contents.end()};
    }

    // Send the response
    res.prepare_payload();
    return res;
  }

  string_response respond404(string_request &&req) {
    string_response res{http::status::not_found, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.keep_alive(req.keep_alive());

    res.set(http::field::content_type, "text/plain");
    res.body() = std::string("Not found");
    res.prepare_payload();
    return res;
  }

  void do_read_upload(const begin_upload &msg) {
    auto &upload = uploads_[msg.url];
    upload.mime_type = msg.mime_type;
    upload.contents.resize(msg.content_length);
    async_read(stream_, asio::buffer(upload.contents),
               asio::transfer_exactly(msg.content_length),
               bind(&self::on_read_upload));
  }

  void on_read_upload(std::error_code ec, std::size_t n) {
    if (ec) {
      std::cerr << "Error reading upload: " << ec.message() << std::endl;
      return;
    }

    do_recv();
  }

  void do_prompt(const prompt &msg) {
    std::ostringstream os;
    os << "http://localhost:" << http_->port() << '/' << session_id_ << msg.url;
    std::cerr << "Opening " << os.str() << std::endl;

    if (ws_) {
      // TODO - needs to account for reconnect across navigation
      json::object navigate;
      navigate["type"] = "navigate";
      navigate["href"] = os.str();
      ws_send_buf_ = json::serialize(navigate);
      ws_->async_write(asio::buffer(ws_send_buf_), bind(&self::on_ws_write));
    } else {
      open_url(os.str());
    }

    do_recv();
  }
};

int main(int argc, char *argv[]) {
  // Check command line arguments.
  if (argc != 2) {
    std::cerr << "Usage: http-server-async <port>\n"
              << "Example:\n"
              << "    " << argv[0] << " 8080\n";
    return EXIT_FAILURE;
  }
  auto const address = asio::ip::make_address("127.0.0.1");
  auto const port = static_cast<unsigned short>(std::atoi(argv[1]));
  // The io_context is required for all I/O
  asio::io_context ioc{};

  // Create and launch a listening port
  auto http =
      std::make_shared<http_listener>(ioc, tcp::endpoint{address, port});
  http->run();

  auto lb = catui_server_fd(stderr);
  if (!lb) {
    return EXIT_FAILURE;
  }

  std::thread th{[&ioc, lb, http]() {
    while (true) {
      int client = catui_server_accept(lb, stderr);
      if (client < 0) {
        std::cerr << "Failed to accept catui client" << std::endl;
        close(client);
        break;
      }

      auto con = std::make_shared<catui_connection>(
          catui_stream{asio::make_strand(ioc), client}, http);

      con->run();
    }
  }};
  th.detach();

  ioc.run();

  return EXIT_SUCCESS;
}
