#include <gtest/gtest.h>

#include <catui.h>
#include <html_forms.h>
#include <html_forms_server.h>
#include <msgstream.h>
#include <unixsocket.h>

#include <unistd.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <regex>
#include <string>
#include <thread>

#define LOG 0

extern const char *test_scratch_dir;
std::mutex log_mtx_;

namespace fs = std::filesystem;
namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

void log(const std::string &s) {
  if (LOG) {
    std::unique_lock lck{log_mtx_};
    std::cerr << s << std::endl;
  }
}

http::response<http::string_body> http_get(const std::string &url);

template <typename Duration> class timer {
  Duration d_;
  std::chrono::system_clock::time_point start_;

public:
  timer(Duration d) : d_{d} { start_ = std::chrono::system_clock::now(); }

  bool expired() {
    auto now = std::chrono::system_clock::now();
    return (now - start_) > d_;
  }
};

class uuid_generator {
private:
  std::uniform_int_distribution<std::uint8_t> rng_;
  std::random_device dev_;

public:
  uuid_generator() : rng_{0, 255}, dev_{} {}

  std::string generate() {
    std::uint8_t bytes[16];
    for (int i = 0; i < 16; ++i) {
      bytes[i] = rng_(dev_);
    }

    char uuid_str[37];
    std::snprintf(
        uuid_str, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
        bytes[7], bytes[8], bytes[9], bytes[10], bytes[11], bytes[12],
        bytes[13], bytes[14], bytes[15]);

    return std::string{uuid_str};
  }
};

class server {
  short port_ = 9876;

  fs::path scratch_;
  fs::path catui_address_;
  int catui_server_;

  uuid_generator uuidgen_;
  fs::path session_dir_;
  html_forms_server *html_server_;
  std::thread server_thread_;

  int client_fd_;

  std::queue<html_forms_server_event> events_;
  std::mutex evt_mtx_;
  std::condition_variable evt_has_data_;

  static void evt_callback(const html_forms_server_event *evt, void *ctx) {
    static_cast<server *>(ctx)->on_event(evt);
  }

  void on_event(const html_forms_server_event *evt) {
    log("queueing server event");
    std::unique_lock lck{evt_mtx_};
    events_.emplace(std::move(*evt));
    evt_has_data_.notify_one();
  }

public:
  server() {
    scratch_ = fs::path{test_scratch_dir};

    if (fs::exists(scratch_)) {
      log("removing " + scratch_.string());
      fs::remove_all(scratch_);
    }

    log("creating " + scratch_.string());
    fs::create_directories(scratch_);

    catui_address_ = scratch_ / "catui.sock";
    setenv("CATUI_ADDRESS", catui_address_.c_str(), 1);

    catui_server_ = unix_socket();
    assert(catui_server_ >= 0);

    assert(unix_bind(catui_server_, catui_address_.c_str()) == 0);

    assert(unix_listen(catui_server_, 8) == 0);

    session_dir_ = scratch_ / "sessions";
    log("creating " + session_dir_.string());
    fs::create_directories(session_dir_);

    log("initializing html server");
    html_server_ = html_forms_server_init(port_, session_dir_.c_str());
    assert(html_server_);

    int ret =
        html_forms_server_set_event_callback(html_server_, &evt_callback, this);
    assert(ret);

    log("Spawning html server thread");
    server_thread_ =
        std::thread{[this] { html_forms_server_run(html_server_); }};
  }

  ~server() {
    log("Stopping html server");
    html_forms_server_stop(html_server_);

    log("Joining server thread");
    server_thread_.join();

    log("freeing server");
    html_forms_server_free(html_server_);

    log("closing catui server socket");
    ::close(catui_server_);

    log("deleting " + scratch_.string());
    fs::remove_all(scratch_);
  };

  std::promise<std::string> accept_client() {
    std::promise<std::string> p;

    std::thread t{[this, &p] {
      log("accepting catui client");
      client_fd_ = unix_accept(catui_server_);
      assert(client_fd_ >= 0);

      char connect_buf[CATUI_CONNECT_SIZE];
      size_t msg_size;
      log("receiving catui connect request");
      int ec = msgstream_fd_recv(client_fd_, connect_buf, CATUI_CONNECT_SIZE,
                                 &msg_size);
      assert(ec == MSGSTREAM_OK);

      catui_connect_request req;
      log("decoding catui connect request");
      assert(catui_decode_connect(connect_buf, msg_size, &req));

      std::string proto{req.protocol};
      assert(proto == "com.gulachek.html-forms");

      log("sending catui ack");
      catui_server_ack(client_fd_, stderr);

      log("connecting accepted fd to html server");
      auto uuid = uuidgen_.generate();
      html_forms_server_start_session(html_server_, uuid.c_str(), client_fd_);

      p.set_value(uuid);
    }};

    t.detach();
    return p;
  }

  short port() const { return port_; }

  void pop_event(html_forms_server_event &evt) {
    log("waiting for server event");
    std::unique_lock lck{evt_mtx_};
    timer t{std::chrono::milliseconds(1000)};
    while (events_.empty() && !t.expired()) {
      evt_has_data_.wait_for(lck, std::chrono::milliseconds{200});
    }

    if (t.expired()) {
      log("timer expired waiting for server event");
      assert(!t.expired());
    }

    log("popping event");
    evt = std::move(events_.front());
    events_.pop();
  }
};

class client {
  html_connection *con_ = nullptr;
  bool transferred_ = false;
  std::string session_id_;
  short server_port_ = 0;

  client() = default;

public:
  client(server &s) {
    server_port_ = s.port();
    log("starting server's accept_client");
    auto accepted = s.accept_client();
    log("calling html_connect");
    html_connect(&con_);
    log("waiting for server to finish accept");
    session_id_ = accepted.get_future().get();
    assert(!session_id_.empty());
    log("server done accepting");
  }

  ~client() {
    if (transferred_) {
      log("skipping disconnect because connection was transferred");
      // don't care about mem leak. Not recommended to transfer existing
      // connection. Could make better by allowing release of fd, or
      // manually do catui_connect and transfer that
    } else {
      log("html_disconnect");
      html_disconnect(con_);
    }
  }

  std::string session_id() const { return session_id_; }

  void navigate(const std::string &url) {
    log("navigating to " + url);
    int rc = html_navigate(con_, url.c_str());
    ASSERT_TRUE(rc);
  }

  void accept_io_transfer(const std::string &token) {
    log("accepting I/O transfer for token " + token);
    int rc = html_accept_io_transfer(con_, token.c_str());
    ASSERT_TRUE(rc);
  }

  void upload_string(const char *url, std::string content) {
    log("uploading " + std::string{url});
    log(content);
    assert(html_upload_stream_open(con_, url));
    assert(html_upload_stream_write(con_, content.data(), content.size()));
    assert(html_upload_stream_close(con_));
  }

  client transfer() {
    log("transferring connection");
    assert(con_);
    int fd = html_connection_fd(con_);
    assert(fd > -1);

    client other;
    int ret = html_connection_transfer_fd(&other.con_, fd);
    assert(ret);
    return other;
  }

  std::string expected_navigation_url(const std::string &path) const {
    std::ostringstream os;
    os << "http://localhost:" << server_port_ << '/' << session_id_ << path;
    return os.str();
  }
};

TEST(HtmlForms, NavigateTriggersServerEvent) {
  server s;
  client c{s};
  html_forms_server_event evt;

  std::string url = "/index.html";
  c.navigate(url);

  s.pop_event(evt);
  EXPECT_EQ(evt.type, HTML_FORMS_SERVER_EVENT_OPEN_URL);
  EXPECT_EQ(c.expected_navigation_url(url), std::string{evt.data.open_url.url});
  EXPECT_EQ(c.session_id(), std::string{evt.data.open_url.session_id});
}

TEST(HtmlForms, AcceptIOTransferTriggersServerEvent) {
  server s;
  client c{s};
  html_forms_server_event evt;

  std::string token = "11111111-2222-3333-4444-555555555555";
  c.accept_io_transfer(token);
  s.pop_event(evt);
  EXPECT_EQ(evt.type, HTML_FORMS_SERVER_EVENT_ACCEPT_IO_TRANSFER);
  EXPECT_EQ(token, std::string{evt.data.accept_io_transfer.token});
  EXPECT_EQ(c.session_id(),
            std::string{evt.data.accept_io_transfer.session_id});
}

TEST(HtmlForms, CanNavigateATransferredConnection) {
  server s;
  client c{s};
  html_forms_server_event evt;

  client c2 = c.transfer();

  c2.navigate("/index.html");
  s.pop_event(evt);
  EXPECT_EQ(evt.type, HTML_FORMS_SERVER_EVENT_OPEN_URL);
}

TEST(HtmlForms, CanRequestUploadedResourceAfterSubsequentNavigate) {
  server s;
  client c{s};
  html_forms_server_event evt;

  c.upload_string("/hello.html", "hello");
  c.navigate("/hello.html");
  s.pop_event(evt);
  ASSERT_EQ(evt.type, HTML_FORMS_SERVER_EVENT_OPEN_URL);

  auto resp = http_get(evt.data.open_url.url);
  EXPECT_EQ(resp.result_int(), 200);
  EXPECT_EQ(resp.body(), "hello");
}

bool parse_url(const std::string &url, std::string &hostname, std::string &port,
               std::string &path) {
  std::smatch match;
  std::regex url_re{R"(http://(localhost):(\d+)(.*))"};
  log("parsing url " + url);
  if (std::regex_match(url, match, url_re)) {
    log("url match!");
    hostname = match[1];
    port = match[2];
    path = match[3];
    return true;
  }

  log("not a url match");
  return false;
}

http::response<http::string_body> http_get(const std::string &url) {
  log("HTTP GET " + url);
  std::string hostname;
  std::string port;
  std::string path;

  assert(parse_url(url, hostname, port, path));

  // from
  // https://www.boost.org/doc/libs/develop/libs/beast/example/http/client/sync/http_client_sync.cpp
  net::io_context ioc;
  tcp::resolver resolver{ioc};
  beast::tcp_stream stream{ioc};

  log("resolving hostname");
  auto const results = resolver.resolve(hostname, port);

  log("connecting to host");
  stream.connect(results);

  http::request<http::string_body> req{http::verb::get, path, 11};
  req.set(http::field::host, hostname);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  log("sending http request");
  http::write(stream, req);

  log("reading response");
  beast::flat_buffer buffer;
  http::response<http::string_body> resp;
  http::read(stream, buffer, resp);

  return resp;
}
