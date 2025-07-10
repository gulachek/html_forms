/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef HTTP_LISTENER_HPP
#define HTTP_LISTENER_HPP

#include "asio-pch.hpp"
#include "my-beast.hpp"
#include <memory>

struct http_session {
  virtual ~http_session() {}
  virtual my::string_response respond(const std::string_view &target,
                                      my::string_request &&req) = 0;

  virtual void connect_ws(boost::asio::ip::tcp::socket &&sock,
                          my::string_request &&req) = 0;
};

// Accepts incoming connections and launches the sessions
class http_listener : public std::enable_shared_from_this<http_listener> {
  boost::asio::io_context &ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::map<std::string, std::weak_ptr<http_session>> sessions_;

public:
  http_listener(boost::asio::io_context &ioc,
                boost::asio::ip::tcp::endpoint endpoint);

  // Start accepting incoming connections
  void run();

  std::uint16_t port() const { return acceptor_.local_endpoint().port(); }

  bool add_session(const std::string &session_id,
                   std::weak_ptr<http_session> session);
  void remove_session(const std::string &session_id);

private:
  void do_accept();
  void on_accept(boost::beast::error_code ec,
                 boost::asio::ip::tcp::socket socket);
};

#endif
