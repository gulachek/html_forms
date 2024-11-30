#include "html_forms/server.h"

#include <catui.h>
#include <msgstream.h>
#include <unixsocket.h>

#include <unistd.h>

#include <boost/json.hpp>

#include <array>
#include <cassert>
#include <iostream>
#include <string_view>
#include <thread>

namespace json = boost::json;

int spawn_browser(int *fd_in, int *fd_out);

#define BROWSER_BUF_SIZE 2048

class event_listener {
private:
  html_forms_server *html_;
  int browser_in_;
  int browser_out_;

  static void event_callback(const html_forms_server_event *ev, void *ctx);

  void on_event(const html_forms_server_event &ev) {
    if (ev.type == HTML_FORMS_SERVER_EVENT_OPEN_URL) {
      const auto &open = ev.data.open_url;
      std::cout << "open " << open.url << std::endl;
      json::object obj;
      obj["type"] = "open";
      obj["url"] = open.url;
      obj["windowId"] = open.window_id;
      send(obj);
    } else if (ev.type == HTML_FORMS_SERVER_EVENT_CLOSE_WINDOW) {
      const auto &close = ev.data.close_win;
      std::cout << "close window " << close.window_id << std::endl;
      json::object obj;
      obj["type"] = "close";
      obj["windowId"] = close.window_id;
      send(obj);
    } else if (ev.type == HTML_FORMS_SERVER_EVENT_SHOW_ERROR) {
      const auto &err = ev.data.show_err;
      std::cout << "show error " << err.msg << std::endl;
      json::object obj;
      obj["type"] = "error";
      obj["windowId"] = err.window_id;
      obj["msg"] = err.msg;
      send(obj);
    } else {
      std::cerr << "Unknown event type '" << ev.type << '\'' << std::endl;
    }
  }

  void send(const json::object &obj) {
    std::string msg = json::serialize(obj);
    if (msg.size() > BROWSER_BUF_SIZE) {
      std::cerr << "Message too big: " << msg << std::endl;
      return;
    }

    msgstream_fd_send(browser_in_, msg.data(), BROWSER_BUF_SIZE, msg.size());
  }

public:
  event_listener(html_forms_server *html) : html_{html} {}

  void listen() {
    html_forms_server_set_event_callback(html_, event_callback, this);

    if (spawn_browser(&browser_in_, &browser_out_) == -1) {
      std::cerr << "Failed to spawn browser process" << std::endl;
      return;
    }

    std::thread browser_th{[this] {
      std::array<char, BROWSER_BUF_SIZE> buf;

      while (true) {
        std::size_t msg_size;
        int ret =
            msgstream_fd_recv(browser_out_, buf.data(), buf.size(), &msg_size);
        if (ret) {
          std::cerr << msgstream_errstr(ret) << std::endl;
          return;
        }

        std::string_view msg{buf.data(), msg_size};
        json::object obj = json::parse(msg).as_object();
        if (obj["type"] == "close") {
          int window_id = obj["windowId"].as_int64();
          std::cout << "Requesting to close window " << window_id << std::endl;
          html_forms_server_close_window(html_, window_id);
        } else {
          std::cerr << "Unknown browser message type: " << obj["type"]
                    << std::endl;
          return;
        }
      }
    }};

    browser_th.detach();
  }
};

void event_listener::event_callback(const html_forms_server_event *ev,
                                    void *ctx) {
  auto evl = static_cast<event_listener *>(ctx);
  assert(!!evl);
  evl->on_event(*ev);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>\n"
              << "Example:\n"
              << "    " << argv[0] << " 8080\n";
    return EXIT_FAILURE;
  }

  auto const port = static_cast<unsigned short>(std::atoi(argv[1]));

  html_forms_server *server = html_forms_server_init(port);
  if (!server) {
    std::cerr << "Failed to initialize server on port " << port << std::endl;
    return 1;
  }

  event_listener ev{server};
  ev.listen();

  int catui_sock = unix_socket();
  if (catui_sock == -1) {
    perror("unix_socket: ");
    return 1;
  }

  const char *sock_path = "build/catui.sock";
  ::unlink(sock_path);

  if (unix_bind(catui_sock, sock_path) == -1) {
    perror("unix_bind: ");
    return 1;
  }

  if (unix_listen(catui_sock, 16) == -1) {
    perror("unix_listen: ");
    return 1;
  }

  std::cout << "Listening at " << sock_path << std::endl;

  std::thread html_forms_th{[server] { html_forms_server_run(server); }};

  char buf[CATUI_CONNECT_SIZE];

  while (true) {
    int con = unix_accept(catui_sock);
    if (con == -1) {
      perror("unix_accept: ");
      return 1;
    }

    size_t msg_size;
    int ec = msgstream_fd_recv(con, buf, sizeof(buf), &msg_size);

    if (ec) {
      std::cerr << "Failed to recv catui connection: " << msgstream_errstr(ec)
                << std::endl;
      ::close(con);
      continue;
    }

#define NOPE(msg)                                                              \
  catui_server_nack(con, msg, stderr);                                         \
  ::close(con);                                                                \
  continue;

    catui_connect_request req;
    if (!catui_decode_connect(buf, msg_size, &req)) {
      NOPE("Invalid connection request format")
    }

    if (!(req.catui_version.major == 0 && req.catui_version.minor == 1)) {
      NOPE("Catui version must be compatible with 0.1.0")
    }

    if (strcmp(req.protocol, "com.gulachek.html-forms") != 0) {

      NOPE("Only com.gulachek.html-forms is a supported protocol")
    }

    if (!(req.version.major == 0 && req.version.minor == 1 &&
          req.version.patch >= 0)) {
      NOPE("Version is not compatible with com.gulachek.html-forms 0.1.0")
    }

    html_forms_server_connect(server, con);
#undef NOPE
  }

  html_forms_th.join();
  return 0;
}

int spawn_browser(int *fd_in, int *fd_out) {
  int pipe_in[2], pipe_out[2];
  if (::pipe(pipe_in) || ::pipe(pipe_out)) {
    ::perror("pipe");
    return -1;
  }

  pid_t pid = ::fork();
  if (pid == -1) {
    ::perror("fork");
    return -1;
  } else if (pid == 0) {
    // child - read in, write out
    ::close(pipe_in[1]);
    ::close(pipe_out[0]);
    if (::dup2(pipe_in[0], STDIN_FILENO) == -1) {
      ::perror("dup2 (stdin)");
      ::exit(EXIT_FAILURE);
    }

    if (::dup2(pipe_out[1], STDOUT_FILENO) == -1) {
      ::perror("dup2 (stdout)");
      ::exit(EXIT_FAILURE);
    }

    std::vector<const char *> argv = BROWSER_ARGS;
    argv.push_back(nullptr); // null terminate
    if (::execvp(BROWSER_EXE, (char *const *)argv.data()) == -1) {
      ::perror("execvp");
      ::exit(EXIT_FAILURE);
    }
  } else {
    // parent - write in, read out
    ::close(pipe_in[0]);
    ::close(pipe_out[1]);
    *fd_in = pipe_in[1];
    *fd_out = pipe_out[0];
  }

  return 0;
}
