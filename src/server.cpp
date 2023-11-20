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
#include "html-forms.h"
#include "http_listener.hpp"

extern "C" {
#include <catui_server.h>
}

namespace asio = boost::asio;
using asio::ip::tcp;

typedef asio::posix::stream_descriptor catui_stream;

class catui_connection : public std::enable_shared_from_this<catui_connection> {
  using self = catui_connection;

  catui_stream stream_;
  std::array<std::uint8_t, CATUI_ACK_SIZE> ack_;
  std::array<std::uint8_t, HTML_MSG_SIZE> html_buf_;

  template <typename... FnArgs>
  auto bind(void (catui_connection::*fn)(FnArgs...)) {
    return std::bind_front(fn, shared_from_this());
  }

public:
  catui_connection(catui_stream &&stream) : stream_{std::move(stream)} {}

  // Start the asynchronous operation
  void run() { asio::dispatch(stream_.get_executor(), bind(&self::do_ack)); }

private:
  void do_ack() {
    auto n = catui_server_encode_ack(ack_.data(), ack_.size(), stderr);
    if (n < 0)
      return;

    async_msgstream_send(stream_, asio::buffer(ack_), n, bind(&self::on_ack));
  }

  void on_ack(std::error_condition ec, msgstream_size n) {
    if (ec) {
      std::cerr << "Error sending ack: " << ec.message() << std::endl;
      return;
    }

    do_recv();
  }

  void do_recv() {
    async_msgstream_recv(stream_, asio::buffer(html_buf_),
                         bind(&self::on_recv));
    return;
    auto n = msgstream_recv(stream_.native_handle(), html_buf_.data(),
                            html_buf_.size(), stderr);
    if (n < 0) {
      return;
    }

    on_recv(std::error_condition{}, n);
  }

  void on_recv(std::error_condition ec, msgstream_size n) {
    if (ec) {
      std::cerr << "Error receiving html message: " << ec.message()
                << std::endl;
      return;
    }

    struct html_msg msg;
    if (!html_decode_msg(html_buf_.data(), n, &msg)) {
      std::cerr << "Error decoding html message" << std::endl;
      return;
    }

    switch (msg.type) {
    case HTML_BEGIN_UPLOAD:
      std::cerr << "Received upload message" << std::endl;
      break;
    case HTML_PROMPT:
      std::cerr << "Received prompt message" << std::endl;
      break;
    default:
      std::cerr << "Invalid message type: " << msg.type << std::endl;
      break;
    }
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
  auto const doc_root = std::make_shared<std::string>(".");

  // The io_context is required for all I/O
  asio::io_context ioc{};

  // Create and launch a listening port
  std::make_shared<http_listener>(ioc, tcp::endpoint{address, port}, doc_root)
      ->run();

  auto lb = catui_server_fd(stderr);
  if (!lb) {
    return EXIT_FAILURE;
  }

  std::thread th{[&ioc, lb]() {
    while (true) {
      int client = catui_server_accept(lb, stderr);
      if (client < 0) {
        std::cerr << "Failed to accept catui client" << std::endl;
        close(client);
        break;
      }

      auto con = std::make_shared<catui_connection>(
          catui_stream{asio::make_strand(ioc), client});

      con->run();
    }
  }};
  th.detach();

  ioc.run();

  return EXIT_SUCCESS;
}
