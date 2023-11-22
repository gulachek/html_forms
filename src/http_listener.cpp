#include "http_listener.hpp"
#include "html-forms.h"
#include "mime_type.hpp"
#include <complex>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace asio = boost::asio;     // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

using session_map = std::map<std::string, std::weak_ptr<http_session>>;

// Report a failure
void fail(beast::error_code ec, char const *what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public std::enable_shared_from_this<session> {
  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  const session_map &sessions_;

public:
  // Take ownership of the stream
  session(tcp::socket &&socket, const session_map &sessions)
      : stream_(std::move(socket)), sessions_{sessions} {}

  // Start the asynchronous operation
  void run() {
    // We need to be executing within a strand to perform async operations
    // on the I/O objects in this session. Although not strictly necessary
    // for single-threaded contexts, this example code is written to be
    // thread-safe by default.
    net::dispatch(
        stream_.get_executor(),
        beast::bind_front_handler(&session::do_read, shared_from_this()));
  }

  void do_read() {
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};

    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    // Read a request
    http::async_read(
        stream_, buffer_, req_,
        beast::bind_front_handler(&session::on_read, shared_from_this()));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == http::error::end_of_stream)
      return do_close();

    if (ec)
      return fail(ec, "read");

    std::string target{req_.target()};
    char session_id[128], normalized_target[256];
    int found =
        html_parse_target(target.c_str(), session_id, sizeof(session_id),
                          normalized_target, sizeof(normalized_target));

    if (!found)
      return respond404("Target path not parsed");

    auto it = sessions_.find(session_id);
    if (it == sessions_.end())
      return respond404("No session");

    if (auto session_ptr = it->second.lock()) {
      send_response(session_ptr->respond(normalized_target, std::move(req_)));
    } else {
      return respond404("Session expired");
    }
  }

  void respond404(const char *msg) {
    http::response<http::string_body> res{http::status::not_found,
                                          req_.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain");
    res.keep_alive(req_.keep_alive());
    res.body() = std::string(msg);
    res.prepare_payload();

    // Send the response
    send_response(std::move(res));
  }

  void send_response(http::message_generator &&msg) {
    bool keep_alive = msg.keep_alive();

    // Write the response
    beast::async_write(stream_, std::move(msg),
                       beast::bind_front_handler(
                           &session::on_write, shared_from_this(), keep_alive));
  }

  void on_write(bool keep_alive, beast::error_code ec,
                std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec)
      return fail(ec, "write");

    if (!keep_alive) {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      return do_close();
    }

    // Read another request
    do_read();
  }

  void do_close() {
    // Send a TCP shutdown
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
  }
};

http_listener::http_listener(asio::io_context &ioc, tcp::endpoint endpoint)
    : ioc_(ioc), acceptor_(asio::make_strand(ioc)) {
  beast::error_code ec;

  // Open the acceptor
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    fail(ec, "open");
    return;
  }

  // Allow address reuse
  acceptor_.set_option(net::socket_base::reuse_address(true), ec);
  if (ec) {
    fail(ec, "set_option");
    return;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    fail(ec, "bind");
    return;
  }

  // Start listening for connections
  acceptor_.listen(net::socket_base::max_listen_connections, ec);
  if (ec) {
    fail(ec, "listen");
    return;
  }
}

// Start accepting incoming connections
void http_listener::run() { do_accept(); }

std::string http_listener::add_session(std::weak_ptr<http_session> session) {
  auto it = sessions_.emplace(std::make_pair("test", session));
  return it.first->first;
}

void http_listener::remove_session(const std::string &session_id) {
  sessions_.erase(session_id);
}

void http_listener::do_accept() {
  // The new connection gets its own strand
  acceptor_.async_accept(
      net::make_strand(ioc_),
      beast::bind_front_handler(&http_listener::on_accept, shared_from_this()));
}

void http_listener::on_accept(beast::error_code ec, tcp::socket socket) {
  if (ec) {
    fail(ec, "accept");
    return; // To avoid infinite loop
  } else {
    // Create the session and run it
    std::make_shared<session>(std::move(socket), sessions_)->run();
  }

  // Accept another connection
  do_accept();
}
