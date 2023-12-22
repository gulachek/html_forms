#include <html-forms.h>

#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  html_connection con = html_connection_alloc();

  if (!html_connect(con)) {
    std::cerr << "Failed to connect to html socket" << std::endl;
    return 1;
  }

  if (!html_upload_dir(con, "/", DOCROOT_PATH)) {
    std::cerr << "Failed to upload docroot" << std::endl;
    return 1;
  }

  if (!html_navigate(con, "/index.html")) {
    std::cerr << "Failed to navigate to /index.html" << std::endl;
    return 1;
  }

  std::vector<char> msg;
  msg.resize(1024);

  html_recv_js_message(con, msg.data(), msg.size());

  html_connection_free(&con);
  return 0;
}
