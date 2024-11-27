#include "html_forms/server.h"

#include <catui.h>
#include <msgstream.h>
#include <unixsocket.h>

#include <unistd.h>

#include <iostream>
#include <thread>

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
  }

  html_forms_th.join();
  return 0;
}
