#define BOOST_TEST_MODULE parse_form_test
#include <boost/test/unit_test.hpp>

#include "html-forms.h"
#include <msgstream.h>

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

    if (msgstream_fd_send(pipe_[1], buf, sizeof(buf), n))
      BOOST_FAIL("Failed to send form header");

    if (write(pipe_[1], sv.data(), sv.size()) != sv.size())
      BOOST_FAIL("Failed to send form content");
  }

  int recv(const std::string_view &sv) {
    send(sv);
    int ret = html_read_form(pipe_[0], &form_);
    if (ret < 0)
      BOOST_FAIL("Failed to read form");

    // Check consistency of object
    BOOST_TEST(html_form_size(form_) == ret);
    for (int i = 0; i < ret; ++i) {
      const char *name_c = html_form_field_name(form_, i);
      std::string_view name{name_c};
      std::string_view value{html_form_field_value(form_, i)};

      BOOST_TEST(value == html_form_value_of(form_, name_c));
    }

    return ret;
  }

  void fail(const std::string_view &sv) {
    send(sv);
    int ret = html_read_form(pipe_[0], &form_);
    BOOST_TEST(ret < 0);
  }

  void chk(const char *name, const char *expected_value) {
    const char *measured_value = html_form_value_of(form_, name);
    if (measured_value == NULL && expected_value != NULL) {
      std::ostringstream ss;
      ss << "Expected to find value for field '" << name
         << "' but no value was found.";
      BOOST_FAIL(ss.str());
    }

    BOOST_TEST(std::string_view{measured_value} == expected_value);
  }

  std::string_view name(int i) { return html_form_field_name(form_, i); }

  std::string_view value(int i) { return html_form_field_value(form_, i); }
};

BOOST_FIXTURE_TEST_CASE(SingleParameter, f) {
  int ret = recv("response=hello");

  BOOST_TEST(ret == 1);
  chk("response", "hello");
}

BOOST_FIXTURE_TEST_CASE(ParsesPlusAsSpace, f) {
  int ret = recv("response=hello+world");

  BOOST_TEST(ret == 1);
  chk("response", "hello world");
}

BOOST_FIXTURE_TEST_CASE(ParsesPercentValue, f) {
  int ret = recv("response=hello%20world");

  BOOST_TEST(ret == 1);
  chk("response", "hello world");
}

BOOST_FIXTURE_TEST_CASE(ParsesMultiplePercentValuesWithHex, f) {
  int ret = recv("t=hey%23there%2a%2Atest%21%21%21%f0%9f%92%a9%F0%9F%92%A9");

  BOOST_TEST(ret == 1);
  chk("t", "hey#there**test!!!💩💩");
}

BOOST_FIXTURE_TEST_CASE(ParsesMultipleValues, f) {
  int ret = recv("apple=red&banana=yellow&pear=greenish+%20yellow");

  BOOST_TEST(ret == 3);
  chk("apple", "red");
  chk("banana", "yellow");
  chk("pear", "greenish  yellow");
}

BOOST_FIXTURE_TEST_CASE(EmptyForm, f) {
  int ret = recv("");
  BOOST_TEST(ret == 0);
}

BOOST_FIXTURE_TEST_CASE(EmptyField, f) {
  int ret = recv("first=1&&third=3");
  BOOST_TEST(ret == 3);
  BOOST_TEST(name(1) == "");
  BOOST_TEST(value(1) == "");
}

BOOST_FIXTURE_TEST_CASE(EmptyValueWithEq, f) {
  int ret = recv("first=1&second=&third=3");
  BOOST_TEST(ret == 3);
  chk("second", "");
}

BOOST_FIXTURE_TEST_CASE(EmptyValueWithoutEq, f) {
  int ret = recv("first=1&second&third=3");
  BOOST_TEST(ret == 3);
  chk("second", "");
}

// TODO - memory leaks exist in parsing failure
BOOST_FIXTURE_TEST_CASE(ErrorToHaveMultipleEq, f) { fail("t=1=2"); }
BOOST_FIXTURE_TEST_CASE(ErrorToHaveInvalidPctChar, f) { fail("t=%2x"); }
BOOST_FIXTURE_TEST_CASE(ErrorToHaveTruncatedPctChar, f) { fail("t=%2"); }
BOOST_FIXTURE_TEST_CASE(ErrorToHaveTruncatedPctChar2, f) { fail("t=%"); }