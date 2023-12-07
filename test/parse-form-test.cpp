#define BOOST_TEST_MODULE parse_form_test
#include <boost/test/unit_test.hpp>

#include "html-forms.h"

struct f {
  int pipe_[2];
  html_form form_ = nullptr;

  f() {
    if (::pipe(pipe_) == -1) {
      BOOST_FAIL("Failed to create pipes");
    }
  }

  ~f() {
    ::close(pipe_[0]);
    ::close(pipe_[1]);
    html_form_release(&form_);
  }

  void send(const std::string_view &sv) {
    char buf[HTML_MSG_SIZE];
    int n = html_encode_submit_form(buf, sizeof(buf), sv.size(),
                                    "application/x-www-form-urlencoded");

    BOOST_REQUIRE(n > 0);

    if (msgstream_send(pipe_[1], buf, sizeof(buf), n, stderr) < 0)
      BOOST_FAIL("Failed to send form header");

    if (write(pipe_[1], sv.data(), sv.size()) != sv.size())
      BOOST_FAIL("Failed to send form content");
  }

  int recv(const std::string_view &sv) {
    send(sv);
    int ret = html_read_form(pipe_[0], &form_);
    if (ret < 0)
      BOOST_FAIL("Failed to read form");

    return ret;
  }

  std::string_view val(const char *name) {
    return html_form_value_of(form_, name);
  }
};

BOOST_FIXTURE_TEST_CASE(TODO_EmptyForm, f) {
  // not implemented
  BOOST_TEST(true);
}

BOOST_FIXTURE_TEST_CASE(SingleParameter, f) {
  int ret = recv("response=hello");

  BOOST_REQUIRE(ret == 1);
  BOOST_TEST(val("response") == "hello");
}

BOOST_FIXTURE_TEST_CASE(ParsesPlusAsSpace, f) {
  int ret = recv("response=hello+world");

  BOOST_REQUIRE(ret == 1);
  BOOST_TEST(val("response") == "hello world");
}

BOOST_FIXTURE_TEST_CASE(ParsesPercentValue, f) {
  int ret = recv("response=hello%20world");

  BOOST_REQUIRE(ret == 1);
  BOOST_TEST(val("response") == "hello world");
}
