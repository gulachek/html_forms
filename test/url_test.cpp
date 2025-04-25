#include <gtest/gtest.h>

#include "html_forms_server/private/parse_target.hpp"

struct ParseTargetUrl : public testing::Test {
protected:
  char session_id_[128];
  char target_path_[256];

  int parse(const char *target) {
    return parse_target(target, session_id_, sizeof(session_id_), target_path_,
                        sizeof(target_path_));
  }

  void test(const char *target, const std::string_view &session_id,
            const std::string_view &normal_path) {
    ASSERT_TRUE(parse(target));
    EXPECT_EQ(session_id, session_id_);
    EXPECT_EQ(normal_path, target_path_);
  }
};

TEST_F(ParseTargetUrl, RootDirNotFound) { EXPECT_FALSE(parse("/")); }

TEST_F(ParseTargetUrl, BasicCaseSessionIdAndTarget) {
  test("sid/foo/bar.txt", "sid", "/foo/bar.txt");
  test("/sid/foo/bar.txt", "sid", "/foo/bar.txt");
}

TEST_F(ParseTargetUrl, AppendsIndexToDir) {
  test("session_id/", "session_id", "/index.html");
  test("session_id", "session_id", "/index.html");
  test("/session_id/", "session_id", "/index.html");
  test("/session_id", "session_id", "/index.html");
  test("/session_id/bar/", "session_id", "/bar/index.html");
  test("/session_id/bar", "session_id", "/bar");
  test("session_id/bar/", "session_id", "/bar/index.html");
}

TEST_F(ParseTargetUrl, MultipleSlashesAreNormalized) {
  test("///sid//foo/////bar.txt", "sid", "/foo/bar.txt");
}

TEST_F(ParseTargetUrl, CurrentDirectoryNormalizedOut) {
  test("/sid/././foo/././././bar.txt", "sid", "/foo/bar.txt");
  test("/sid/././foo/./././.", "sid", "/foo/index.html");
}

TEST_F(ParseTargetUrl, HiddenFilesNotFound) {
  EXPECT_FALSE(parse("/sid/.foo"));
  EXPECT_FALSE(parse("/sid/..foo"));
  EXPECT_FALSE(parse("/sid/...foo"));
  EXPECT_FALSE(parse("/sid/bar/baz/.foo"));
}

TEST_F(ParseTargetUrl, ParentDirectoryNormalizedOut) {
  test("/sid/foo/../bar.txt", "sid", "/bar.txt");
  test("/sid/foo/../../../bar.txt", "sid", "/bar.txt");
  test("/sid/../../../bar.txt", "sid", "/bar.txt");
  test("/sid/../bar.txt/..", "sid", "/index.html");
  test("/sid/../bar.txt/../", "sid", "/index.html");
}

TEST_F(ParseTargetUrl, MoreDotsInFileNameNotFound) {
  EXPECT_FALSE(parse("/sid/..."));
  EXPECT_FALSE(parse("/sid/.../"));
  EXPECT_FALSE(parse("/sid/foo/..../../"));
}

TEST_F(ParseTargetUrl, VirtualAbsolutePath) {
  test("/sid/~", "sid", "/index.html");
  test("/sid/~/", "sid", "/index.html");
  test("/sid/foo/~/bar.txt", "sid", "/bar.txt");
  test("/sid/foo/bar/baz/~/qux.txt", "sid", "/qux.txt");
  test("/sid/foo/bar/~/baz/~/qux.txt", "sid", "/qux.txt");
}

TEST_F(ParseTargetUrl, TildeMisuseNotFound) {
  EXPECT_FALSE(parse("/sid/foo~/bar.txt"));
  EXPECT_FALSE(parse("/sid/foo~baz/bar.txt"));
  EXPECT_FALSE(parse("/sid/~baz/bar.txt"));
  EXPECT_FALSE(parse("/sid/foo~"));
  EXPECT_FALSE(parse("/sid/foo~baz"));
  EXPECT_FALSE(parse("/sid/~baz"));
}

TEST_F(ParseTargetUrl, SpecialCharsInSessionIdOk) {
  test("/sid~~123/foo/bar.txt", "sid~~123", "/foo/bar.txt");
  test("/sid~/hello/", "sid~", "/hello/index.html");
  test("/..sid./foo/~/bar.txt", "..sid.", "/bar.txt");
  test("...sid/foo/bar/baz/~/qux.txt", "...sid", "/qux.txt");
}

TEST_F(ParseTargetUrl, ReservedCharsNotFound) {
  EXPECT_FALSE(parse("/sid/bar%20.txt")); // TODO support escape
  EXPECT_FALSE(parse("/sid/bar+.txt"));   // TODO support escape
  EXPECT_FALSE(parse("/sid/bar/hello@ampersand/bar.txt"));
}
