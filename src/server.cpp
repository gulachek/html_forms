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
#include "browser.hpp"
#include "html-forms.h"
#include "http_listener.hpp"
#include "mime_type.hpp"
#include "my-asio.hpp"
#include "my-beast.hpp"
#include "open-url.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <filesystem>

extern "C" {
#include <catui.h>
}

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using asio::ip::tcp;

template <typename Fn, typename Class>
concept member_fn_of =
    std::is_member_function_pointer_v<Fn> &&
    requires(Fn fp, Class *inst) { std::bind_front(fp, inst); };

class catui_connection : public std::enable_shared_from_this<catui_connection>,
                         public http_session,
                         public browser::window_watcher {
  using self = catui_connection;

  my::stream_descriptor stream_;
  std::vector<std::uint8_t> buf_;
  std::shared_ptr<http_listener> http_;
  std::string session_id_;
  std::vector<std::uint8_t> submit_buf_;
  beast::flat_buffer ws_buf_;
  std::string ws_send_buf_;
  browser &browser_;
  browser::window_id window_id_;

  std::map<std::string, std::string> mime_overrides_;
  boost::uuids::name_generator_sha1 name_gen_{boost::uuids::ns::url()};
  const std::filesystem::path &all_sessions_dir_;
  std::filesystem::path docroot_;

  std::shared_ptr<my::ws_stream> ws_;

  template <member_fn_of<self> Fn, typename... FnArgs>
  auto bind(Fn &&fn, FnArgs &&...args) {
    return std::bind_front(fn, shared_from_this(),
                           std::forward<FnArgs>(args)...);
  }

public:
  catui_connection(my::stream_descriptor &&stream,
                   const std::shared_ptr<http_listener> &http, browser &browsr,
                   const std::filesystem::path &all_sessions_dir)
      : stream_{std::move(stream)}, http_{http}, browser_{browsr},
        all_sessions_dir_{all_sessions_dir} {}

  ~catui_connection() {
    std::cerr << "Destructor for session " << session_id_ << std::endl;
    http_->remove_session(session_id_);
    browser_.release_window(window_id_);

    std::filesystem::remove_all(docroot_);
  }

  // Start the asynchronous operation
  void run() {
    // TODO - thread safety on http_ ptr
    session_id_ = http_->add_session(weak_from_this());
    // TODO - weak from this
    window_id_ = browser_.reserve_window(weak_from_this());

    docroot_ = all_sessions_dir_ / session_id_;
    std::filesystem::create_directory(docroot_);
    std::filesystem::permissions(docroot_, std::filesystem::perms::owner_all);

    asio::dispatch(stream_.get_executor(), bind(&self::do_ack));
  }

  my::string_response respond(const std::string_view &target,
                              my::string_request &&req) override {

    switch (req.method()) {
    case http::verb::post:
      return respond_post(target, std::move(req));
    case http::verb::head:
    case http::verb::get:
      return respond_get(target, std::move(req));
    default:
      break;
    }

    my::string_response res{http::status::bad_request, req.version()};
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
                  my::string_request &&req) override {
    if (ws_) {
      std::cerr << "Aborting websocket connection because one already exists "
                   "for the session"
                << std::endl;
      sock.close();
      return;
    }

    ws_ = my::make_ws_ptr(std::move(sock));
    my::async_ws_accept(*ws_, req, bind(&self::on_ws_accept));
  }

  void window_close_requested() override {
    asio::post(stream_.get_executor(), bind(&self::request_close));
  }

private:
  void do_ack() {
    buf_.resize(CATUI_ACK_SIZE);
    auto n = catui_server_encode_ack(buf_.data(), buf_.size(), stderr);
    if (n < 0)
      return;

    my::async_msgstream_send(stream_, asio::buffer(buf_), n,
                             bind(&self::on_ack));
  }

  void on_ack(std::error_condition ec, std::size_t n) {
    if (ec) {
      std::cerr << "Error sending ack: " << ec.message() << std::endl;
      return end_catui();
    }

    buf_.resize(HTML_MSG_SIZE);
    do_recv();
  }

  void do_recv() {
    asio::dispatch([this] {
      stream_.get_executor(),
          my::async_msgstream_recv(stream_, asio::buffer(buf_),
                                   bind(&self::on_recv));
    });
  }

  void on_recv(std::error_condition ec, std::size_t n) {
    if (ec) {
      std::cerr << "Error receiving html message: " << ec.message()
                << std::endl;
      return end_catui();
    }

    struct html_out_msg msg;
    if (!html_decode_out_msg(buf_.data(), n, &msg)) {
      std::cerr << "Error decoding html message" << std::endl;
      return;
    }

    switch (msg.type) {
    case HTML_BEGIN_UPLOAD:
      do_read_upload(msg.msg.upload);
      break;
    case HTML_NAVIGATE:
      do_navigate(msg.msg.navigate);
      break;
    case HTML_JS_MESSAGE:
      do_send_js_msg(msg.msg.js_msg);
      break;
    case HTML_MIME_MAP:
      do_map_mimes(msg.msg.mime);
      break;
    default:
      std::cerr << "Invalid message type: " << msg.type << std::endl;
      break;
    }
  }

  void do_map_mimes(html_mime_map mimes) {
    std::size_t n = html_mime_map_size(mimes);
    for (std::size_t i = 0; i < n; ++i) {
      const char *ext_c, *mime_c;
      if (!html_mime_map_entry_at(mimes, i, &ext_c, &mime_c)) {
        std::cerr << "failed to read mime entry" << std::endl;
        break;
      }

      std::cerr << '[' << session_id_ << "] MIME ." << ext_c << " -> " << mime_c
                << std::endl;
      mime_overrides_[ext_c] = mime_c;
    }

    do_recv();
  }

  void request_close() {
    std::cerr << '[' << session_id_ << "] CLOSE-REQ" << std::endl;
    submit_buf_.resize(HTML_MSG_SIZE);
    int msg_size =
        html_encode_close_request(submit_buf_.data(), submit_buf_.size());
    if (msg_size < 0) {
      std::cerr << "Failed to encode close msg" << std::endl;
      return end_ws();
    }

    my::async_msgstream_send(stream_, asio::buffer(submit_buf_), msg_size,
                             bind(&self::on_request_close));
  }

  void on_request_close(std::error_condition ec, std::size_t n) {
    if (ec) {
      std::cerr << "Failed to send close request: " << ec.message()
                << std::endl;
      return end_catui();
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

  void end_ws() {
    if (!ws_)
      return;

    ws_ = nullptr;
  }

  void end_catui() {
    stream_.close();

    if (ws_) {
      ws_->close(beast::websocket::close_code::none);
    }
  }

  void do_ws_read() {
    if (!ws_) {
      std::cerr << "Invalid do_ws_read with no websocket connection"
                << std::endl;
      return;
    }

    my::async_ws_read(*ws_, ws_buf_, bind(&self::on_ws_read));
  }

  void on_ws_read(beast::error_code ec, std::size_t size) {
    if (ec) {
      std::cerr << "Failed to read ws message for session " << session_id_
                << std::endl;
      return end_ws();
    }

    auto msg = beast::buffers_to_string(ws_buf_.data());
    ws_buf_.consume(size);

    auto val = json::parse(msg);
    if (!val.is_object()) {
      std::cerr << "WS message is not a JSON object" << std::endl;
      return end_ws();
    }

    // TODO validate this more
    auto &obj = val.as_object();
    auto &type = obj["type"].as_string();
    if (type == "recv") {
      auto &js_msg = obj["msg"].as_string();
      do_send_recv_msg(js_msg);
    } else {
      std::cerr << "Invalid WS message type: " << type << std::endl;
      return end_ws();
    }
  }

  void do_send_recv_msg(json::string &msg) {
    std::cerr << '[' << session_id_ << "] RECV: " << msg << std::endl;

    // TODO - need to sync w/ POST
    submit_buf_.resize(HTML_MSG_SIZE);
    if (html_encode_recv_js_msg(submit_buf_.data(), submit_buf_.size(),
                                msg.size()) < 0) {
      std::cerr << "Failed to encode recv msg" << std::endl;
      return end_ws();
    }

    asio::dispatch(stream_.get_executor(),
                   bind(&self::submit_recv_js_msg,
                        std::make_shared<std::string>(std::move(msg))));
  }

  void submit_recv_js_msg(std::shared_ptr<std::string> msg) {
    auto buf = asio::buffer(submit_buf_.data(), HTML_MSG_SIZE);
    my::async_msgstream_send(stream_, buf, submit_buf_.size(),
                             bind(&self::on_submit_recv_js_msg, msg));
  }

  void on_submit_recv_js_msg(std::shared_ptr<std::string> msg,
                             std::error_condition ec, std::size_t n) {
    if (ec) {
      std::cerr << "Failed to send RECV msg" << std::endl;
      return end_catui();
    }

    asio::async_write(stream_, asio::buffer(*msg),
                      asio::transfer_exactly(msg->size()),
                      bind(&self::on_send_recv_js_msg_content, msg));
  }

  void on_send_recv_js_msg_content(std::shared_ptr<std::string> msg,
                                   std::error_code ec, std::size_t n) {
    if (ec) {
      std::cerr << "Failed to send RECV msg content" << std::endl;
      return end_catui();
    }

    do_ws_read();
  }

  void on_ws_write(beast::error_code ec, std::size_t size) {
    if (ec) {
      std::cerr << "Failed to send ws message for session " << session_id_
                << std::endl;
      return end_ws();
    }

    do_recv();
  }

  my::string_response respond_post(const std::string_view &target,
                                   my::string_request &&req) {
    if (target == "/submit") {
      std::string ctype = req[http::field::content_type];
      if (ctype != "application/x-www-form-urlencoded") {
        return respond400("Invalid content type", std::move(req));
      }

      if (req.body().size() > HTML_FORM_SIZE) {
        return respond400("Form too big", std::move(req));
      }

      submit_buf_.resize(HTML_MSG_SIZE);
      if (!html_encode_submit_form(submit_buf_.data(), submit_buf_.size(),
                                   req.body().size(), ctype.c_str())) {
        return respond400("Failed to encode form submission", std::move(req));
      }

      std::cerr << "Initiating post with body: " << req.body() << std::endl;
      asio::dispatch(stream_.get_executor(),
                     bind(&self::submit_post, std::make_shared<std::string>(
                                                  std::move(req.body()))));

      my::string_response res{http::status::ok, req.version()};
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

  void submit_post(std::shared_ptr<std::string> body) {
    std::cerr << "Posting body: " << *body << std::endl;
    auto buf = asio::buffer(submit_buf_.data(), HTML_MSG_SIZE);
    my::async_msgstream_send(stream_, buf, submit_buf_.size(),
                             bind(&self::on_submit_post, body));
  }

  void on_submit_post(std::shared_ptr<std::string> body,
                      std::error_condition ec, std::size_t n) {
    if (ec) {
      std::cerr << "Error sending form to app: " << ec.message() << std::endl;
      return end_catui();
    }

    asio::async_write(stream_, asio::buffer(body->data(), body->size()),
                      asio::transfer_exactly(body->size()),
                      bind(&self::on_write_form, body));
  }

  void on_write_form(std::shared_ptr<std::string> body, std::error_code ec,
                     std::size_t n) {
    if (ec) {
      std::cerr << "Error sending form contents to app: " << ec.message()
                << std::endl;
      return end_catui();
    }
  }

  std::filesystem::path upload_path(const std::string_view &url) const {
    auto uuid = name_gen_(url.data(), url.size());
    std::ostringstream os;
    os << uuid;
    auto path = docroot_ / os.str();
    return path;
  }

  std::string_view mime_type_for(const std::string_view &url) const {
    auto start = url.rfind('.');
    if (start == std::string_view::npos)
      return std::string_view{};

    // don't wastefully compare the dot
    start += 1;

    std::string ext;
    ext.resize(url.size() - start);

    for (auto i = 0; i < ext.size(); ++i) {
      ext[i] = std::tolower(url[start + i]);
    }

    auto mime_it = mime_overrides_.find(ext);
    if (mime_it == mime_overrides_.end())
      return mime_type(ext);
    else
      return mime_it->second;
  }

  my::string_response respond_get(const std::string_view &target,
                                  my::string_request &&req) {
    my::string_response res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.keep_alive(req.keep_alive());

    auto path = upload_path(target);

    if (!std::filesystem::exists(path))
      return respond404(std::move(req));

    auto mime = mime_type_for(target);
    if (mime.empty())
      return respond404(std::move(req));

    res.set(http::field::content_type, mime);

    auto size = std::filesystem::file_size(path);
    res.content_length(size);

    // Respond to HEAD request
    if (req.method() == http::verb::head) {
      res.body() = "";
      return res;
    }

    if (req.method() == http::verb::get) {
      std::ifstream upload_file{path};
      std::string upload_content;
      upload_content.resize(size);
      upload_file.read(upload_content.data(), size);
      upload_file.close();
      res.body() = std::move(upload_content);
    }

    // Send the response
    res.prepare_payload();
    return res;
  }

  my::string_response respond404(my::string_request &&req) {
    my::string_response res{http::status::not_found, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.keep_alive(req.keep_alive());

    res.set(http::field::content_type, "text/plain");
    res.body() = std::string("Not found");
    res.prepare_payload();
    return res;
  }

  my::string_response respond400(const std::string_view &msg,
                                 my::string_request &&req) {
    my::string_response res{http::status::bad_request, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.keep_alive(req.keep_alive());

    res.set(http::field::content_type, "text/plain");
    res.body() = std::string(msg);
    res.prepare_payload();
    return res;
  }

  void do_read_upload(const begin_upload &msg) {
    auto contents = std::make_shared<std::string>();
    contents->resize(msg.content_length);
    std::cerr << '[' << session_id_ << "] UPLOAD " << msg.url << std::endl;

    my::async_readn(stream_, asio::buffer(*contents), msg.content_length,
                    bind(&self::on_read_upload, msg.is_archive,
                         std::make_shared<std::string>(msg.url), contents));
  }

  void on_read_upload(bool is_archive, std::shared_ptr<std::string> url,
                      std::shared_ptr<std::string> contents, std::error_code ec,
                      std::size_t n) {
    if (ec) {
      std::cerr << "Error reading upload: " << ec.message() << std::endl;
      return end_catui();
    }

    if (is_archive) {
      struct archive *a;
      a = archive_read_new();
      archive_read_support_filter_all(a);
      archive_read_support_format_all(a);
      int r = archive_read_open_memory(a, contents->data(), contents->size());
      if (r != ARCHIVE_OK) {
        std::cerr << "Failed to open archive " << archive_error_string(a)
                  << std::endl;
        return end_catui(); // fatal
      }

      struct archive_entry *entry;
      while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        mode_t type = archive_entry_filetype(entry);
        if (type != AE_IFREG) // only upload regular files
          continue;

        std::ostringstream cat_url_ss;
        cat_url_ss << *url;

        std::string_view entry_pathname{archive_entry_pathname(entry)};

        if (!(url->ends_with('/') || entry_pathname.starts_with('/')))
          cat_url_ss << '/';

        cat_url_ss << entry_pathname;
        auto cat_url = cat_url_ss.str();
        std::cerr << '[' << session_id_ << "] UPLOAD-ENTRY " << cat_url
                  << std::endl;

        auto path = upload_path(cat_url);
        std::ofstream of{path};

        const void *buffer;
        std::size_t size;
        std::int64_t offset;

        while (true) {
          r = archive_read_data_block(a, &buffer, &size, &offset);
          if (r == ARCHIVE_EOF) {
            break;
          }
          if (r < ARCHIVE_OK) {
            std::cerr << "Error reading entry contents: "
                      << archive_error_string(a) << std::endl;
            return end_catui();
          }

          of.write(static_cast<const char *>(buffer), size);
        }

        of.close();
      }

      archive_read_free(a);

    } else {
      auto path = upload_path(*url);
      std::ofstream of{path};
      // TODO - error handling
      of.write(contents->data(), n);
      of.close();
    }

    do_recv();
  }

  void do_navigate(const navigate &msg) {
    std::ostringstream os;
    os << "http://localhost:" << http_->port() << '/' << session_id_ << msg.url;
    std::cerr << "Opening " << os.str() << std::endl;

    browser_.async_load_url(window_id_, os.str(), bind(&self::on_load_url));
  }

  void on_load_url(std::error_condition ec) {
    if (ec) {
      std::cerr << "Error opening window: " << ec.message() << std::endl;
      return;
    }

    do_recv();
  }

  void do_send_js_msg(const js_message &msg) {
    ws_send_buf_.resize(msg.content_length);
    my::async_readn(stream_, asio::buffer(ws_send_buf_), msg.content_length,
                    bind(&self::on_recv_js_msg_content));
  }

  void on_recv_js_msg_content(std::error_code ec, std::size_t n) {
    if (ec) {
      return end_catui();
    }

    std::cerr << '[' << session_id_ << "] SEND: " << ws_send_buf_ << std::endl;

    json::object msg;
    msg["type"] = "send";
    msg["msg"] = std::move(ws_send_buf_);
    ws_send_buf_ = json::serialize(msg);
    my::async_ws_write(*ws_, asio::buffer(ws_send_buf_),
                       bind(&self::on_ws_write));
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

  // make sure we have a directory to work with
  auto temp_dir = std::filesystem::temp_directory_path();
  auto session_dir = temp_dir / "com.gulachek.html-forms" / "session-content";
  std::filesystem::create_directories(session_dir);

  std::cerr << "[server] Writing content to " << session_dir << std::endl;

  // The io_context is required for all I/O
  asio::io_context ioc{};

  browser browsr{ioc};
  browsr.run();

  // Create and launch a listening port
  auto http =
      std::make_shared<http_listener>(ioc, tcp::endpoint{address, port});
  http->run();

  auto lb = catui_server_fd(stderr);
  if (!lb) {
    return EXIT_FAILURE;
  }

  std::thread th{[&ioc, lb, http, &browsr, &session_dir]() {
    while (true) {
      int client = catui_server_accept(lb, stderr);
      if (client < 0) {
        std::cerr << "Failed to accept catui client" << std::endl;
        close(client);
        break;
      }

      auto con = std::make_shared<catui_connection>(
          my::stream_descriptor{asio::make_strand(ioc), client}, http, browsr,
          session_dir);

      con->run();
    }
  }};
  th.detach();

  ioc.run();

  return EXIT_SUCCESS;
}
