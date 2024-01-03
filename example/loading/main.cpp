#include <html_forms.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

using std::chrono::milliseconds;

int main(int argc, char **argv) {
  html_connection *con;
  if (!html_connect(&con)) {
    std::cerr << "Failed to connect: " << html_errmsg(con) << std::endl;
    return 1;
  }

  if (!html_upload_dir(con, "/", DOCROOT_PATH)) {
    std::cerr << "Failed to upload docroot: " << html_errmsg(con) << std::endl;
    return 1;
  }

  while (true) {
    if (!html_navigate(con, "/index.html")) {
      std::cerr << "Failed to navigate: " << html_errmsg(con) << std::endl;
      return 1;
    }

    html_form *form;
    auto ec = html_read_form(con, &form);
    if (ec) {
      if (ec == HTML_CLOSE_REQ) {
        return 0;
      } else {
        std::cerr << "Failed to read form: " << html_errmsg(con) << std::endl;
        return 1;
      }
    }

    const char *delay_ms_cstr = html_form_value_of(form, "delay-ms");
    if (!delay_ms_cstr) {
      std::cerr << "Missing 'delay-ms' field in submitted form" << std::endl;
      return 1;
    }

    auto delay_ms_raw = std::strtoul(delay_ms_cstr, NULL, 10);
    auto delay_ms = std::clamp(delay_ms_raw, 0UL, 15UL * 1000UL);
    if (delay_ms != delay_ms_raw) {
      std::cerr << delay_ms_raw << "ms is out of range. Restricting to "
                << delay_ms << "ms" << std::endl;
    }

    std::cout << "Sleeping for " << delay_ms << "ms" << std::endl;
    std::this_thread::sleep_for(milliseconds{delay_ms});
  }

  return 1;
}
