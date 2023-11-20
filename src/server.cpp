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
#include "http_listener.hpp"

extern "C" {
#include <catui_server.h>
}

namespace asio = boost::asio;
namespace beast = boost::beast;
using asio::ip::tcp;

typedef asio::posix::stream_descriptor catui_stream;

class catui_connection : public std::enable_shared_from_this<catui_connection> {
  catui_stream stream_;
  std::array<std::uint8_t, CATUI_ACK_SIZE> ack_;

public:
  catui_connection(catui_stream &&stream) : stream_{std::move(stream)} {}

  // Start the asynchronous operation
  void run() {
    asio::dispatch(stream_.get_executor(),
                   beast::bind_front_handler(&catui_connection::do_read,
                                             shared_from_this()));
  }

  void do_read() {
    using namespace std::placeholders;

    auto n = catui_server_encode_nack(ack_.data(), ack_.size(),
                                      (char *)"Not implemented", nullptr);
    std::cerr << "Encoded catui header with " << n << " bytes" << std::endl;

    auto cb = std::bind(&catui_connection::on_read, shared_from_this(), _1, _2);
    async_msgstream_send(stream_, asio::buffer(ack_), n, cb);
  }

  void on_read(std::error_condition ec, msgstream_size n) {
    if (ec) {
      std::cerr << "Error sending nack: " << ec.message() << std::endl;
      return;
    }

    std::cerr << "on_read" << std::endl;
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
