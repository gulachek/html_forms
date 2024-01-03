#define BOOST_TEST_MODULE escape_string_test
#include <boost/test/unit_test.hpp>

#include "html_forms.h"
#include <string_view>
#include <vector>

struct f {
  f() {}

  ~f() {}

  void chk(const char *in, const char *expect_out) {
    std::vector<char> out;
    out.resize(html_escape_size(in));

    auto size = html_escape(out.data(), out.size(), in);

    std::string_view out_sv{out.data(), out.size() - 1};

    BOOST_TEST(size == out.size());
    BOOST_TEST(out_sv == expect_out);
  }
};

BOOST_FIXTURE_TEST_CASE(EmptyString, f) { chk("", ""); }
BOOST_FIXTURE_TEST_CASE(NullEscapesToEmptyString, f) { chk(NULL, ""); }

BOOST_FIXTURE_TEST_CASE(SimpleStringUnchanged, f) { chk("hello", "hello"); }

BOOST_FIXTURE_TEST_CASE(Ampersand, f) {
  chk("he&ll&&o", "he&amp;ll&amp;&amp;o");
}

BOOST_FIXTURE_TEST_CASE(DQuote, f) { chk("h\"ello\"", "h&quot;ello&quot;"); }

BOOST_FIXTURE_TEST_CASE(SQuote, f) { chk("h'ello'", "h&#039;ello&#039;"); }

BOOST_FIXTURE_TEST_CASE(LessThan, f) { chk("<hello<<", "&lt;hello&lt;&lt;"); }

BOOST_FIXTURE_TEST_CASE(GreaterThan, f) {
  chk(">hello>>", "&gt;hello&gt;&gt;");
}

BOOST_AUTO_TEST_CASE(EscapeSizeIncludesNullTerminator) {
  BOOST_TEST(html_escape_size("") == 1);
}

BOOST_AUTO_TEST_CASE(TooSmallDstSizeReturnsEscapeSize) {
  std::string_view escaped = "&lt;hello&gt;";

  char dst[8];
  size_t n = html_escape(dst, sizeof(dst), "<hello>");

  std::string_view dst_sv{dst, sizeof(dst)};
  BOOST_TEST(n == (escaped.size() + 1));
}
