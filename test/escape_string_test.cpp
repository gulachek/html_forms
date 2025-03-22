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

#define T(NM, IN, OUT)                                                         \
  TEST_F(HtmlEscape, NM) { chk(IN, OUT); }

T(EmptyString, "", "")
T(NullEscapesToEmptyString, NULL, "")
T(SimpleStringUnchanged, "hello", "hello")
T(Ampersand, "he&ll&&o", "he&amp;ll&amp;&amp;o")
T(DQuote, "h\"ello\"", "h&quot;ello&quot;")
T(SQuote, "h'ello'", "h&#039;ello&#039;")
T(LessThan, "<hello<<", "&lt;hello&lt;&lt;")
T(GreaterThan, ">hello>>", "&gt;hello&gt;&gt;")

#undef T

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
