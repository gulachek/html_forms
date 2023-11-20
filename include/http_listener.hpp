#ifndef HTTP_LISTENER_HPP
#define HTTP_LISTENER_HPP

#include "asio-pch.hpp"

// Accepts incoming connections and launches the sessions
class http_listener : public std::enable_shared_from_this<http_listener> {
  boost::asio::io_context &ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::shared_ptr<std::string const> doc_root_;

public:
  http_listener(boost::asio::io_context &ioc,
                boost::asio::ip::tcp::endpoint endpoint,
                std::shared_ptr<std::string const> const &doc_root);

  // Start accepting incoming connections
  void run();

private:
  void do_accept();
  void on_accept(boost::beast::error_code ec,
                 boost::asio::ip::tcp::socket socket);
};

#endif
