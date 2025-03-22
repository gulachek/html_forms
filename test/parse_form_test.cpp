#include <gtest/gtest.h>

#include "html_connection.h"
#include "html_forms.h"
#include "html_forms/encoding.h"
#include <msgstream.h>

class FormParsing : public testing::Test {
protected:
  int pipe_[2];
  html_form *form_ = nullptr;
  html_connection *con_ = nullptr;

  void SetUp() override {
    if (::pipe(pipe_) == -1) {
      ADD_FAILURE() << "Failed to create pipes";
    }

    // TODO - should have a simple parsing function instead of
    // full blown connection
    con_ = (html_connection *)malloc(sizeof(html_connection));
    con_->fd = pipe_[0]; // for reading forms
  }

  void TearDown() override {
    ::close(pipe_[0]);
    ::close(pipe_[1]);
    html_form_free(form_);
    html_disconnect(con_);
  }

  void send(const std::string_view &sv) {
    char buf[HTML_MSG_SIZE];
    int n = html_encode_imsg_form(buf, sizeof(buf), sv.size(),
                                  "application/x-www-form-urlencoded");

    ASSERT_GT(n, 0);

    if (msgstream_fd_send(pipe_[1], buf, sizeof(buf), n))
      FAIL() << "Failed to send form header";

    if (write(pipe_[1], sv.data(), sv.size()) != sv.size())
      FAIL() << "Failed to send form content";
  }

  int recv(const std::string_view &sv) {
    send(sv);
    int ret = html_form_read(con_, &form_);
    if (!ret)
      ADD_FAILURE() << "Failed to read form";

    // Check consistency of object
    std::size_t n = html_form_size(form_);
    for (int i = 0; i < n; ++i) {
      const char *name_c = html_form_name_at(form_, i);
      std::string_view name{name_c};
      std::string_view value{html_form_value_at(form_, i)};

      EXPECT_EQ(value, html_form_value_of(form_, name_c));
    }

    return ret;
  }

  void fail(const std::string_view &sv) {
    send(sv);
    int ret = html_form_read(con_, &form_);
    EXPECT_FALSE(ret);
  }

  void chksz(std::size_t expected_size) {
    EXPECT_EQ(html_form_size(form_), expected_size);
  }

  void chk(const char *name, const char *expected_value) {
    const char *measured_value = html_form_value_of(form_, name);
    if (measured_value == NULL && expected_value != NULL) {
      std::ostringstream ss;
      ss << "Expected to find value for field '" << name
         << "' but no value was found.";
      FAIL() << ss.str();
    }

    EXPECT_EQ(std::string_view{measured_value}, expected_value);
  }

  std::string_view name(int i) { return html_form_name_at(form_, i); }

  std::string_view value(int i) { return html_form_value_at(form_, i); }
};

TEST_F(FormParsing, SingleParameter) {
  int ret = recv("response=hello");

  chksz(1);
  chk("response", "hello");
}

TEST_F(FormParsing, ParsesPlusAsSpace) {
  int ret = recv("response=hello+world");

  chksz(1);
  chk("response", "hello world");
}

TEST_F(FormParsing, ParsesPercentValue) {
  int ret = recv("response=hello%20world");

  chksz(1);
  chk("response", "hello world");
}

TEST_F(FormParsing, ParsesMultiplePercentValuesWithHex) {
  int ret = recv("t=hey%23there%2a%2Atest%21%21%21%f0%9f%92%a9%F0%9F%92%A9");

  chksz(1);
  chk("t", "hey#there**test!!!💩💩");
}

TEST_F(FormParsing, ParsesMultipleValues) {
  int ret = recv("apple=red&banana=yellow&pear=greenish+%20yellow");

  chksz(3);
  chk("apple", "red");
  chk("banana", "yellow");
  chk("pear", "greenish  yellow");
}

TEST_F(FormParsing, EmptyForm) {
  int ret = recv("");
  chksz(0);
}

TEST_F(FormParsing, EmptyField) {
  int ret = recv("first=1&&third=3");
  chksz(3);
  EXPECT_EQ(name(1), "");
  EXPECT_EQ(value(1), "");
}

TEST_F(FormParsing, EmptyValueWithEq) {
  int ret = recv("first=1&second=&third=3");
  chksz(3);
  chk("second", "");
}

TEST_F(FormParsing, EmptyValueWithoutEq) {
  int ret = recv("first=1&second&third=3");
  chksz(3);
  chk("second", "");
}

// TODO - memory leaks exist in parsing failure
TEST_F(FormParsing, ErrorToHaveMultipleEq) { fail("t=1=2"); }
TEST_F(FormParsing, ErrorToHaveInvalidPctChar) { fail("t=%2x"); }
TEST_F(FormParsing, ErrorToHaveTruncatedPctChar) { fail("t=%2"); }
TEST_F(FormParsing, ErrorToHaveTruncatedPctChar2) { fail("t=%"); }
