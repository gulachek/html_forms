#ifndef HTTP_LISTENER_HPP
#define HTTP_LISTENER_HPP

#include "asio-pch.hpp"
#include <memory>

using string_response =
    boost::beast::http::response<boost::beast::http::string_body>;
using string_request =
    boost::beast::http::request<boost::beast::http::string_body>;

struct http_session {
  virtual ~http_session() {}
  virtual string_response respond(const std::string_view &target,
                                  string_request &&req) = 0;
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

  std::string add_session(std::weak_ptr<http_session> session);
  void remove_session(const std::string &session_id);

private:
  void do_accept();
  void on_accept(boost::beast::error_code ec,
                 boost::asio::ip::tcp::socket socket);
};

#endif
