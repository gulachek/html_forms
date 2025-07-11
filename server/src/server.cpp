/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#include "html_forms_server.h"
#include "html_forms_server/private/asio-pch.hpp"
#include "html_forms_server/private/browser.hpp"
#include "html_forms_server/private/http_listener.hpp"
#include "html_forms_server/private/mime_type.hpp"
#include "html_forms_server/private/my-asio.hpp"
#include "html_forms_server/private/my-beast.hpp"
#include "html_forms_server/private/session_lock.hpp"
#include <boost/system/detail/errc.hpp>
#include <html_forms.h>
#include <html_forms/encoding.h>

#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <boost/endian/arithmetic.hpp>
#include <filesystem>
#include <pwd.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include <catui.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using asio::ip::tcp;

template <typename Fn, typename Class>
concept member_fn_of =
    std::is_member_function_pointer_v<Fn> &&
    requires(Fn fp, Class *inst) { std::bind_front(fp, inst); };

struct read_upload_state {
  std::filesystem::path path;
  std::string url;
  std::ofstream of;
  html_resource_type rtype;
  bool is_stream;
  boost::endian::little_uint32_at chunk_size;
  std::size_t chunk_bytes_left;

  bool has_more_chunks() const { return is_stream && chunk_size > 0; }
};

class catui_connection : public std::enable_shared_from_this<catui_connection>,
                         public http_session,
                         public browser::window_watcher {
  using self = catui_connection;

  my::stream_descriptor stream_;
  std::vector<std::uint8_t> output_msg_buf_;
  std::shared_ptr<http_listener> http_;
  std::string session_id_;
  std::vector<std::uint8_t> submit_buf_;
  beast::flat_buffer ws_buf_;
  std::string ws_send_buf_;
  browser &browser_;
  bool gracefully_closed_ = false;

  std::map<std::string, std::string> mime_overrides_;
  boost::uuids::name_generator_sha1 name_gen_{boost::uuids::ns::url()};
  const std::filesystem::path &all_sessions_dir_;
  session_lock session_mtx_;
  std::filesystem::path docroot_;
  std::filesystem::path archives_dir_;
  std::filesystem::path files_dir_;

  std::shared_ptr<my::ws_stream> ws_;

  template <member_fn_of<self> Fn, typename... FnArgs>
  auto bind(Fn &&fn, FnArgs &&...args) {
    return std::bind_front(fn, shared_from_this(),
                           std::forward<FnArgs>(args)...);
  }

  std::ostream &log() { return std::cerr << '[' << session_id_ << "] "; }

public:
  catui_connection(my::stream_descriptor &&stream, const char *session_id,
                   const std::shared_ptr<http_listener> &http, browser &browsr,
                   const std::filesystem::path &all_sessions_dir)
      : stream_{std::move(stream)}, session_id_{session_id}, http_{http},
        browser_{browsr}, all_sessions_dir_{all_sessions_dir} {}

  ~catui_connection() {
    http_->remove_session(session_id_);

    if (!gracefully_closed_) {
      browser_.show_error(
          session_id_, "Session terminated. This is likely due to poor "
                       "connection quality, killing a process, or a software "
                       "bug.");
    }

    std::filesystem::remove_all(docroot_);
  }

  // Start the asynchronous operation
  void run() {

    // TODO - thread safety on http_ ptr
    http_->add_session(session_id_, weak_from_this());
    browser_.add_session(session_id_, weak_from_this());

    docroot_ = all_sessions_dir_ / session_id_;

#define MKDIR std::filesystem::create_directory
    MKDIR(docroot_);

    // TODO nack or fatal_error
    if (!session_mtx_.open(docroot_)) {
      std::cerr << "Failed to open session lock" << std::endl;
      return;
    }

    if (!session_mtx_.try_lock()) {
      ::perror("sem_wait");
      std::cerr << "Failed to obtain session lock" << std::endl;
      return;
    }

    std::filesystem::permissions(docroot_, std::filesystem::perms::owner_all);

    auto uploads = docroot_ / "uploads";
    MKDIR(uploads);
    files_dir_ = uploads / "files";
    MKDIR(files_dir_);
    archives_dir_ = uploads / "archives";
    MKDIR(archives_dir_);
#undef MKDIR

    asio::dispatch(stream_.get_executor(), bind(&self::do_recv));
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
      log() << "Aborting websocket connection because one already exists "
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
  void do_recv() {
    log() << "Waiting to receive html message" << std::endl;

    output_msg_buf_.resize(HTML_MSG_SIZE);
    asio::dispatch([this] {
      stream_.get_executor(),
          my::async_msgstream_recv(stream_, asio::buffer(output_msg_buf_),
                                   bind(&self::on_recv));
    });
  }

  void on_recv(std::error_condition ec, std::size_t n) {
    if (ec) {
      log() << "Error receiving html message: " << ec.message() << std::endl;
      return end_catui();
    }

    struct html_out_msg msg;
    if (!html_decode_out_msg(output_msg_buf_.data(), n, &msg)) {
      return fatal_error("Invalid output message");
    }

    switch (msg.type) {
    case HTML_OMSG_UPLOAD:
      do_read_upload(msg.msg.upload);
      break;
    case HTML_OMSG_NAVIGATE:
      do_navigate(msg.msg.navigate);
      break;
    case HTML_OMSG_APP_MSG:
      do_send_app_msg(msg.msg.app_msg);
      break;
    case HTML_OMSG_MIME_MAP:
      do_map_mimes(msg.msg.mime);
      break;
    case HTML_OMSG_CLOSE:
      do_close();
      break;
    case HTML_OMSG_ACCEPT_IO_TRANSFER:
      do_accept_io_transfer(msg.msg.accept_io_transfer);
      break;
    default:
      log() << "Invalid message type: " << msg.type << std::endl;
      break;
    }
  }

  void do_close() {
    log() << "CLOSE" << std::endl;
    gracefully_closed_ = true;
    end_catui();
    browser_.remove_session(session_id_);
  }

  void do_map_mimes(html_mime_map *mimes) {
    std::size_t n = html_mime_map_size(mimes);
    for (std::size_t i = 0; i < n; ++i) {
      const char *ext_c, *mime_c;
      if (!html_mime_map_entry_at(mimes, i, &ext_c, &mime_c)) {
        std::cerr << "failed to read mime entry" << std::endl;
        break;
      }

      log() << "MIME ." << ext_c << " -> " << mime_c << std::endl;
      mime_overrides_[ext_c] = mime_c;
    }

    html_mime_map_free(mimes);
    do_recv();
  }

  void request_close() {
    log() << "CLOSE-REQ" << std::endl;
    submit_buf_.resize(HTML_MSG_SIZE);
    int msg_size =
        html_encode_imsg_close_req(submit_buf_.data(), submit_buf_.size());
    if (msg_size < 0) {
      return fatal_error("Failed to encode close message");
    }

    my::async_msgstream_send(stream_, asio::buffer(submit_buf_), msg_size,
                             bind(&self::on_request_close));
  }

  void on_request_close(std::error_condition ec, std::size_t n) {
    if (ec) {
      log() << "Failed to send close request: " << ec.message() << std::endl;
      return end_catui();
    }
  }

  void on_ws_accept(beast::error_code ec) {
    if (ec) {
      log() << "Failed to accept websocket: " << ec.message() << std::endl;
      return;
    }

    log() << "Websocket connected" << std::endl;
    do_ws_read();
  }

  void end_ws() {
    if (!ws_)
      return;

    ws_ = nullptr;
  }

  void end_catui() { stream_.close(); }

  void do_ws_read() {
    if (!ws_) {
      log() << "Invalid do_ws_read with no websocket connection" << std::endl;
      return;
    }

    my::async_ws_read(*ws_, ws_buf_, bind(&self::on_ws_read));
  }

  void on_ws_read(beast::error_code ec, std::size_t size) {
    if (ec) {
      if (ec == beast::websocket::error::closed) {
        log() << "Failed to read ws message for session " << session_id_ << ": "
              << ec.message() << std::endl;
      }

      return end_ws();
    }

    auto msg = beast::buffers_to_string(ws_buf_.data());
    ws_buf_.consume(size);

    do_send_recv_msg(msg);
  }

  void do_send_recv_msg(std::string &msg) {
    log() << "RECV: " << msg << std::endl;

    // TODO - need to sync w/ POST
    submit_buf_.resize(HTML_MSG_SIZE);
    if (html_encode_imsg_app_msg(submit_buf_.data(), submit_buf_.size(),
                                 msg.size()) < 0) {
      log() << "Failed to encode recv msg" << std::endl;
      return end_ws();
    }

    asio::dispatch(stream_.get_executor(),
                   bind(&self::submit_recv_app_msg,
                        std::make_shared<std::string>(std::move(msg))));
  }

  void submit_recv_app_msg(std::shared_ptr<std::string> msg) {
    auto buf = asio::buffer(submit_buf_.data(), HTML_MSG_SIZE);
    my::async_msgstream_send(stream_, buf, submit_buf_.size(),
                             bind(&self::on_submit_recv_app_msg, msg));
  }

  void on_submit_recv_app_msg(std::shared_ptr<std::string> msg,
                              std::error_condition ec, std::size_t n) {
    if (ec) {
      log() << "Failed to send RECV msg" << std::endl;
      return end_catui();
    }

    asio::async_write(stream_, asio::buffer(*msg),
                      asio::transfer_exactly(msg->size()),
                      bind(&self::on_send_recv_app_msg_content, msg));
  }

  void on_send_recv_app_msg_content(std::shared_ptr<std::string> msg,
                                    std::error_code ec, std::size_t n) {
    if (ec) {
      log() << "Failed to send RECV msg content" << std::endl;
      return end_catui();
    }

    do_ws_read();
  }

  void on_ws_write(beast::error_code ec, std::size_t size) {
    if (ec) {
      log() << "Failed to send ws message for session " << session_id_
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
      if (!html_encode_imsg_form(submit_buf_.data(), submit_buf_.size(),
                                 req.body().size(), ctype.c_str())) {
        return respond400("Failed to encode form submission", std::move(req));
      }

      log() << "Initiating post with body: " << req.body() << std::endl;
      asio::dispatch(stream_.get_executor(),
                     bind(&self::submit_post, std::make_shared<std::string>(
                                                  std::move(req.body()))));

      my::string_response res{http::status::see_other, req.version()};
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.keep_alive(req.keep_alive());
      res.set(http::field::location, "/html/loading.html");
      res.content_length(2);
      res.body() = "ok";
      res.prepare_payload();
      return res;
    } else {
      return respond404(std::move(req));
    }
  }

  void fatal_error(const std::string &msg) {
    log() << "Fatal error: " << msg << std::endl;

    std::shared_ptr<std::string> err_buf = std::make_shared<std::string>();
    err_buf->resize(HTML_MSG_SIZE);

    if (html_encode_imsg_error(err_buf->data(), err_buf->size(), msg.c_str()) <
        0)
      return;

    auto buf = asio::buffer(*err_buf);
    my::async_msgstream_send(stream_, buf, err_buf->size(),
                             bind(&self::on_send_err, err_buf));
  }

  void on_send_err(std::shared_ptr<std::string> err_buf,
                   const std::error_condition &ec, std::size_t n) {
    end_catui();
  }

  void submit_post(std::shared_ptr<std::string> body) {
    log() << "Posting body: " << *body << std::endl;
    auto buf = asio::buffer(submit_buf_.data(), HTML_MSG_SIZE);
    my::async_msgstream_send(stream_, buf, submit_buf_.size(),
                             bind(&self::on_submit_post, body));
  }

  void on_submit_post(std::shared_ptr<std::string> body,
                      std::error_condition ec, std::size_t n) {
    if (ec) {
      log() << "Error sending form to app: " << ec.message() << std::endl;
      return end_catui();
    }

    asio::async_write(stream_, asio::buffer(body->data(), body->size()),
                      asio::transfer_exactly(body->size()),
                      bind(&self::on_write_form, body));
  }

  void on_write_form(std::shared_ptr<std::string> body, std::error_code ec,
                     std::size_t n) {
    if (ec) {
      log() << "Error sending form contents to app: " << ec.message()
            << std::endl;
      return end_catui();
    }
  }

  std::filesystem::path
  upload_path(const std::string_view &url,
              html_resource_type rtype = HTML_RT_FILE) const {
    auto uuid = name_gen_(url.data(), url.size());
    std::ostringstream os;
    os << uuid;

    switch (rtype) {
    case HTML_RT_ARCHIVE:
      return archives_dir_ / os.str();
    case HTML_RT_FILE:
      return files_dir_ / os.str();
    default:
      throw std::logic_error("unhandled resource type");
    }
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

  void do_read_upload(const html_omsg_upload &msg) {
    log() << "UPLOAD " << msg.url << std::endl;

    auto state = std::make_shared<read_upload_state>();
    state->path = upload_path(msg.url, msg.rtype);
    state->rtype = msg.rtype;
    state->url = msg.url;

    state->of.open(state->path);

    if (!state->of)
      return fatal_error("Error opening file for upload");

    state->chunk_bytes_left = msg.content_length;
    state->is_stream = msg.content_length == 0;

    if (state->is_stream)
      read_upload_chunk_size(state);
    else
      read_upload_chunk(state);
  }

  void read_upload_chunk_size(std::shared_ptr<read_upload_state> state) {
    my::async_readn(stream_, asio::buffer(&state->chunk_size, 2), 2,
                    bind(&self::on_read_upload_chunk_size, state));
  }

  void on_read_upload_chunk_size(std::shared_ptr<read_upload_state> state,
                                 std::error_code ec, std::size_t n) {
    if (ec)
      return fatal_error(ec.message());

    state->chunk_bytes_left = state->chunk_size;
    read_upload_chunk(state);
  }

  void read_upload_chunk(std::shared_ptr<read_upload_state> state) {
    std::size_t n = std::min(state->chunk_bytes_left, output_msg_buf_.size());

    my::async_readn(stream_, asio::buffer(output_msg_buf_), n,
                    bind(&self::on_read_upload_chunk, state));
  }

  void on_read_upload_chunk(std::shared_ptr<read_upload_state> state,
                            std::error_code ec, std::size_t n) {
    if (ec)
      return fatal_error(ec.message());

    try {
      state->of.write((const char *)output_msg_buf_.data(), n);
    } catch (const std::exception &ex) {
      return fatal_error(ex.what());
    }

    state->chunk_bytes_left -= n;
    if (state->chunk_bytes_left > 0) {
      read_upload_chunk(state);
    } else if (state->has_more_chunks()) {
      read_upload_chunk_size(state);
    } else {
      state->of.close();
      switch (state->rtype) {
      case HTML_RT_ARCHIVE:
        on_read_archive(state);
        break;
      case HTML_RT_FILE:
        // nothing to do :)
        break;
      default:
        break;
      }

      do_recv();
    }
  }

  void on_read_archive(const std::shared_ptr<read_upload_state> &state) {
    struct archive *a;
    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    int r = archive_read_open_filename(a, state->path.c_str(), 10240);
    if (r != ARCHIVE_OK) {
      log() << "Failed to open archive " << archive_error_string(a)
            << std::endl;
      return end_catui(); // fatal
    }

    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
      mode_t type = archive_entry_filetype(entry);
      if (type != AE_IFREG) // only upload regular files
        continue;

      std::ostringstream cat_url_ss;
      cat_url_ss << state->url;

      std::string_view entry_pathname{archive_entry_pathname(entry)};

      if (!(state->url.ends_with('/') || entry_pathname.starts_with('/')))
        cat_url_ss << '/';

      cat_url_ss << entry_pathname;
      auto cat_url = cat_url_ss.str();
      log() << "UPLOAD-ENTRY " << cat_url << std::endl;

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
          log() << "Error reading entry contents: " << archive_error_string(a)
                << std::endl;
          return end_catui();
        }

        of.write(static_cast<const char *>(buffer), size);
      }

      of.close();
    }

    archive_read_free(a);
    // don't need to keep archive since we wrote the contents
    std::filesystem::remove(state->path);
  }

  void do_navigate(const html_omsg_navigate &msg) {
    std::ostringstream os;
    os << "http://localhost:" << http_->port() << '/' << session_id_ << msg.url;
    log() << "Opening " << os.str() << std::endl;

    browser_.load_url(session_id_, os.str());
    do_recv();
  }

  void do_accept_io_transfer(const html_omsg_accept_io_transfer &msg) {
    log() << "Accepting I/O transfer" << std::endl;
    browser_.accept_io_transfer(session_id_, msg.token);
    do_recv();
  }

  void do_send_app_msg(const html_omsg_app_msg &msg) {
    ws_send_buf_.resize(msg.content_length);
    my::async_readn(stream_, asio::buffer(ws_send_buf_), msg.content_length,
                    bind(&self::on_recv_app_msg_content));
  }

  void on_recv_app_msg_content(std::error_code ec, std::size_t n) {
    if (ec) {
      return end_catui();
    }

    if (!ws_) {
      log() << "Invalid SEND with no websocket connection" << std::endl;
      return;
    }

    log() << "SEND: " << ws_send_buf_ << std::endl;

    my::async_ws_write(*ws_, asio::buffer(ws_send_buf_),
                       bind(&self::on_ws_write));
  }
};

struct html_forms_server_ {
private:
  unsigned short port_;
  asio::io_context ioc_;
  browser browser_;
  std::filesystem::path session_dir_;
  std::shared_ptr<http_listener> http_;

public:
  html_forms_server_(unsigned short port, const char *session_dir)
      : port_{port}, session_dir_{session_dir}, ioc_{}, browser_{} {
    auto const address = asio::ip::make_address("127.0.0.1");
    http_ =
        std::make_shared<http_listener>(ioc_, tcp::endpoint{address, port_});
  }

  int start() {
    std::filesystem::create_directories(session_dir_);

    std::cerr << "[server] Writing content to " << session_dir_ << std::endl;

    // Create and launch a listening port
    http_->run();

    std::thread cleanup{[this]() {
      for (const auto &entry :
           std::filesystem::directory_iterator{session_dir_}) {
        const auto &session_path = entry.path();

        session_lock mtx{session_path};
        if (!mtx.try_lock())
          continue;

        auto session_id = session_path.filename();
        try {
          std::cerr << "Cleaning up inactive session " << session_id
                    << std::endl;
          std::filesystem::remove_all(session_path);
        } catch (const std::exception &ex) {
          std::cerr << "Failed to clean up session: " << ex.what() << std::endl;
        }

        mtx.unlock();
      }
    }};

    cleanup.detach();
    ioc_.run();

    return EXIT_SUCCESS;
  }

  int stop() {
    ioc_.stop();
    return 1;
  }

  int start_session(const char *session_id, int client) {
    auto con = std::make_shared<catui_connection>(
        my::stream_descriptor{asio::make_strand(ioc_), client}, session_id,
        http_, browser_, session_dir_);

    con->run();
    return 1;
  }

  void close_window(const std::string &session_id) {
    asio::dispatch(
        ioc_.get_executor(),
        std::bind(&browser::request_close, &browser_, std::string{session_id}));
  }

  void set_ev_callback(html_forms_server_event_callback *cb, void *ctx) {
    browser_.set_event_callback(cb, ctx);
  }
};

html_forms_server *html_forms_server_init(unsigned short port,
                                          const char *session_dir) {
  return new html_forms_server_(port, session_dir);
}

void html_forms_server_free(html_forms_server *server) {
  if (server) {
    delete server;
  }
}

int html_forms_server_run(html_forms_server *server) {
  if (!server) {
    return 0;
  }

  return server->start();
}

int html_forms_server_stop(html_forms_server *server) {
  if (!server) {
    return 0;
  }

  return server->stop();
}

int html_forms_server_start_session(html_forms_server *server,
                                    const char *session_id, int fd) {
  if (!server)
    return 0;

  return server->start_session(session_id, fd);
}

int html_forms_server_set_event_callback(html_forms_server *server,
                                         html_forms_server_event_callback *cb,
                                         void *ctx) {
  server->set_ev_callback(cb, ctx);
  return 1;
}

int html_forms_server_close_window(html_forms_server *server,
                                   const char *session_id) {
  if (!server)
    return 0;

  server->close_window(session_id);
  return 1;
}
