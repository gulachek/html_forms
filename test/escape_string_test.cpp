#include <gtest/gtest.h>

#include "html_forms.h"
#include <string_view>
#include <vector>

class HtmlEscape : public testing::Test {
protected:
  void chk(const char *in, const char *expect_out) {
    std::vector<char> out;
    out.resize(html_escape_size(in));

    auto size = html_escape(out.data(), out.size(), in);

    std::string_view out_sv{out.data(), out.size() - 1};

    EXPECT_EQ(size, out.size());
    EXPECT_EQ(out_sv, expect_out);
  }
};

TEST_F(HtmlEscape, EmptyString) { chk("", ""); }

TEST_F(HtmlEscape, NullEscapesToEmptyString) { chk(NULL, ""); }

TEST_F(HtmlEscape, SimpleStringUnchanged) { chk("hello", "hello"); }

TEST_F(HtmlEscape, Ampersand) { chk("he&ll&&o", "he&amp;ll&amp;&amp;o"); }

TEST_F(HtmlEscape, DQuote) { chk("h\"ello\"", "h&quot;ello&quot;"); }

TEST_F(HtmlEscape, SQuote) { chk("h'ello'", "h&#039;ello&#039;"); }

TEST_F(HtmlEscape, LessThan) { chk("<hello<<", "&lt;hello&lt;&lt;"); }

TEST_F(HtmlEscape, GreaterThan) { chk(">hello>>", "&gt;hello&gt;&gt;"); }

TEST(SizeChecks, EscapeSizeIncludesNullTerminator) {
  EXPECT_EQ(html_escape_size(""), 1);
}

TEST(SizeChecks, TooSmallDstSizeReturnsEscapeSize) {
  std::string_view escaped = "&lt;hello&gt;";

  char dst[8];
  size_t n = html_escape(dst, sizeof(dst), "<hello>");

  std::string_view dst_sv{dst, sizeof(dst)};
  EXPECT_EQ(n, (escaped.size() + 1));
}
