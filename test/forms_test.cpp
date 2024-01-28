#define BOOST_TEST_MODULE forms_test
#include <boost/test/unit_test.hpp>
#include <cstdlib>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <thread>

#include "html_connection.h"
#include "html_forms.h"
#include "html_forms/encoding.h"

struct f {
  html_connection *con_ = nullptr;
  pid_t catuid_pid_;

  f() {
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
  }

  ~f() {
    ::kill(catuid_pid_, SIGINT);
    ::wait(nullptr);
  }
};

BOOST_FIXTURE_TEST_CASE(HasValueInSubmittedFormField, f) {
  html_upload_stream_open(con_, "/index.html");
  const char *contents = R"(
<!DOCTYPE html>
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
}
