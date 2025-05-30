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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
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

class server {
  fs::path scratch_;
  fs::path catui_address_;
  int catui_server_;

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
    html_server_ = html_forms_server_init(9876, session_dir_.c_str());
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

  std::promise<bool> accept_client() {
    std::promise<bool> p;

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
      html_forms_server_connect(html_server_, client_fd_);

      p.set_value(true);
    }};

    t.detach();
    return p;
  }

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

  client() = default;

public:
  client(server &s) {
    log("starting server's accept_client");
    auto accepted = s.accept_client();
    log("calling html_connect");
    html_connect(&con_);
    log("waiting for server to finish accept");
    assert(accepted.get_future().get());
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

  void navigate(const char *url) {
    log("navigating to " + std::string{url});
    int rc = html_navigate(con_, url);
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
};

TEST(HtmlForms, NavigateTriggersServerEvent) {
  server s;
  client c{s};
  html_forms_server_event evt;

  c.navigate("/index.html");
  s.pop_event(evt);
  EXPECT_EQ(evt.type, HTML_FORMS_SERVER_EVENT_OPEN_URL);
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
