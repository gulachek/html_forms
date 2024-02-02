#define BOOST_TEST_MODULE forms_test
#include <boost/test/unit_test.hpp>
#include <cstdlib>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <thread>

#include <unistd.h>

#include "html_connection.h"
#include "html_forms.h"
#include "html_forms/encoding.h"

void replace(std::string &s, const std::string_view &search,
             const std::string_view &replace) {
  auto i = s.find(search);
  if (i == std::string::npos)
    throw std::logic_error{"Failed to find search expression"};

  s.replace(i, search.size(), replace);
}

struct server {
  pid_t catuid_pid_;
  html_connection *con_ = nullptr;

  server() {
    ::unlink(CATUI_ADDRESS);

    const char *catuid = std::getenv("CATUID");
    if (!catuid) {
      BOOST_FAIL("CATUID env variable not defined");
      return;
    }

    ::setenv("CATUI_ADDRESS", CATUI_ADDRESS, 1);

    auto ret = ::fork();
    if (ret == -1) {
      BOOST_FAIL(::strerror(errno));
      return;
    } else if (ret == 0) {
      ::execl(catuid, catuid, "-s", CATUI_ADDRESS, "-p", CATUI_DIR,
              (const char *)NULL);
      std::perror("execl");
      std::abort();
    } else {
      catuid_pid_ = ret;
    }

    bool connected = false;
    for (int i = 0; i < 100; ++i) {
      if (html_connect(&con_)) {
        connected = true;
        break;
      }

      std::cerr << "Connection attempt " << i << std::endl;
      std::chrono::milliseconds ms{10};
      std::this_thread::sleep_for(ms);
    }

    if (!connected) {
      std::ostringstream os;
      os << "Failed to connect to catuid " << html_errmsg(con_);
      BOOST_FAIL(os.str());
    }

    html_upload_stream_open(con_, "/index.html");
    const char *content = "<h1>Testing in progress</h1>";
    html_upload_stream_write(con_, content, strlen(content));
    html_upload_stream_close(con_);
    html_navigate(con_, "/index.html");
  }

  ~server() {
    html_disconnect(con_);
    ::kill(catuid_pid_, SIGINT);
    ::wait(nullptr);
  }
};

BOOST_TEST_GLOBAL_FIXTURE(server);

struct f {
  html_connection *con_ = nullptr;
  std::filesystem::path content_dir_;

  f() {
    if (!html_connect(&con_)) {
      BOOST_FAIL("Failed to connect to server");
      return;
    }

    content_dir_ = std::filesystem::path{CONTENT_DIR};
  }

  void render(const std::string &s, const std::string &url = "/index.html") {
    if (!html_upload_stream_open(con_, url.c_str())) {
      BOOST_FAIL("Failed to open upload stream: " << html_errmsg(con_));
    }

    if (!html_upload_stream_write(con_, s.data(), s.size())) {
      BOOST_FAIL("Failed to upload contents: " << html_errmsg(con_));
    }

    if (!html_upload_stream_close(con_)) {
      BOOST_FAIL("Failed to close upload stream: " << html_errmsg(con_));
    }

    if (!html_navigate(con_, url.c_str())) {
      BOOST_FAIL("Failed to navigate to /index.html: " << html_errmsg(con_));
    }
  }

  bool confirm(const std::string &msg) {
    std::ostringstream os;
    os << "<script src=\"/html/forms.js\"></script>"
          "<form>"
       << "<h1>" << msg << "</h1>"
       << "<div><button name=\"val\" value=\"yes\">Yes</button>"
          "<button name=\"val\" value=\"no\">No</button>"
          "</form>";

    render(os.str());
    html_form *form;
    if (!html_form_read(con_, &form)) {
      BOOST_FAIL("Failed to read confirm for '" << msg
                                                << "': " << html_errmsg(con_));
    }

    std::string val{html_form_value_of(form, "val")};
    return val == "yes";
  }

  std::string recv() {
    char buf[4096];
    std::size_t n;
    if (!html_recv(con_, buf, sizeof(buf), &n)) {
      BOOST_FAIL("Failed to receive message: " << html_errmsg(con_));
    }

    return std::string{buf, buf + n};
  }

  void send(const std::string &msg) {
    if (!html_send(con_, msg.data(), msg.size())) {
      BOOST_FAIL("Failed to send message: " << html_errmsg(con_));
    }
  }

  ~f() { html_disconnect(con_); }
};

BOOST_FIXTURE_TEST_CASE(HasValueInSubmittedFormField, f) {
  html_upload_stream_open(con_, "/index.html");
  const char *contents = R"(<!DOCTYPE html>
<html>
<head>
<script src="/html/forms.js"></script>
</head>
<body>
<form>
<input type="text" value="value" name="field" />
<button id="click-me">Click Me</button>
</form>
<script>
window.onload = () => {
const btn = document.getElementById('click-me');
btn.click();
}
</script>
</body>
</html>
)";

  html_upload_stream_write(con_, contents, strlen(contents));
  html_upload_stream_close(con_);

  html_navigate(con_, "/index.html");

  html_form *form;
  if (!html_form_read(con_, &form)) {
    BOOST_FAIL("Form not read");
    return;
  }

  const char *field_c = html_form_value_of(form, "field");
  if (!field_c)
    BOOST_FAIL("field form field not found");

  std::string_view field = field_c;
  BOOST_TEST(field == "value");
  html_form_free(form);
}

BOOST_FIXTURE_TEST_CASE(UploadIndividualFiles, f) {
  auto docroot = content_dir_ / "basic";
  auto index = docroot / "index.html";
  auto css = docroot / "main.css";
  auto js = docroot / "main.js";

  html_upload_file(con_, "/index.html", index.c_str());
  html_upload_file(con_, "/main.css", css.c_str());
  html_upload_file(con_, "/main.js", js.c_str());

  html_navigate(con_, "/index.html");

  html_form *form;
  if (!html_form_read(con_, &form)) {
    BOOST_FAIL("Form not read");
    return;
  }

  std::string_view color = html_form_value_of(form, "bg-color");
  BOOST_TEST(color == "rgb(238, 255, 238)");
  html_form_free(form);
}

BOOST_FIXTURE_TEST_CASE(UploadDirectories, f) {
  auto docroot = content_dir_ / "basic";

  html_upload_dir(con_, "/", docroot.c_str());

  html_navigate(con_, "/index.html");

  html_form *form;
  if (!html_form_read(con_, &form)) {
    BOOST_FAIL("Form not read");
    return;
  }

  std::string_view color = html_form_value_of(form, "bg-color");
  BOOST_TEST(color == "rgb(238, 255, 238)");
  html_form_free(form);
}

BOOST_FIXTURE_TEST_CASE(UploadTarGz, f) {
  auto archive = content_dir_ / "basic.tar.gz";

  html_upload_archive(con_, "/", archive.c_str());

  html_navigate(con_, "/index.html");

  html_form *form;
  if (!html_form_read(con_, &form)) {
    BOOST_FAIL("Form not read");
    return;
  }

  std::string_view color = html_form_value_of(form, "bg-color");
  BOOST_TEST(color == "rgb(238, 255, 238)");
  html_form_free(form);
}

BOOST_FIXTURE_TEST_CASE(ClickingCloseXRequestsClose, f) {
  render("<h1> Press the close X</h1>");

  html_form *form;
  int ret = html_form_read(con_, &form);
  BOOST_TEST(!ret, "Expected html_form_read to return falsy");
  BOOST_TEST(html_close_requested(con_));

  html_reject_close(con_);
  BOOST_TEST(!html_close_requested(con_));
}

BOOST_FIXTURE_TEST_CASE(AbruptDisconnectShowsErrorMessage, f) {
  render("<h1> Expect an error message above this </h1>");

  ::close(con_->fd);

  if (!html_connect(&con_)) {
    BOOST_FAIL("Failed to connect to server again: " << html_errmsg(con_));
  }

  BOOST_TEST(confirm("Is there a 'disconnect' error popup?"));
}

BOOST_FIXTURE_TEST_CASE(MimeMapByFileExtension, f) {
  auto docroot = content_dir_ / "basic";
  auto index = docroot / "index.html";
  auto css = docroot / "main.css";
  auto js = docroot / "main.js";

  // odd extension swap to prove that this is respected
  html_upload_file(con_, "/main.js", css.c_str());
  html_upload_file(con_, "/main.css", js.c_str());

  auto n = std::filesystem::file_size(index);
  std::string contents;
  contents.resize(n);
  std::ifstream file{index};
  if (!file) {
    BOOST_FAIL("Failed to open file " << index);
  }

  file.read(contents.data(), n);
  ::replace(contents, "main.css", "main.temp");
  ::replace(contents, "main.js", "main.css");
  ::replace(contents, "main.temp", "main.js");

  // NOW THE FUN PART
  html_mime_map *mimes = html_mime_map_create();
  if (!mimes) {
    BOOST_FAIL("Failed to allocate mime map");
  }

  html_mime_map_add(mimes, ".test", "text/html");
  html_mime_map_add(mimes, ".css", "text/javascript");
  html_mime_map_add(mimes, ".js", "text/css");

  if (!html_mime_map_apply(con_, mimes)) {
    BOOST_FAIL("Failed to apply mime map: " << html_errmsg(con_));
  }

  html_mime_map_free(mimes);

  render(contents, "/index.test");

  html_form *form;
  if (!html_form_read(con_, &form)) {
    BOOST_FAIL("Form not read: " << html_errmsg(con_));
  }

  std::string_view color = html_form_value_of(form, "bg-color");
  BOOST_TEST(color == "rgb(238, 255, 238)");
  html_form_free(form);
}

BOOST_FIXTURE_TEST_CASE(AppMessagesOverWebsocket, f) {
  auto docroot = content_dir_ / "msg";

  html_upload_dir(con_, "/", docroot.c_str());

  html_navigate(con_, "/index.html");

  auto start = recv();
  BOOST_TEST(start == "start!");

  // now it echoes
  send("first");
  BOOST_TEST(recv() == "first");

  send("second");
  BOOST_TEST(recv() == "second");
}
