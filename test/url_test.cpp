#define BOOST_TEST_MODULE url_test
#include <boost/test/unit_test.hpp>

#include "html-forms.h"

struct f {
  char session_id_[128];
  char target_path_[256];

  int parse(const char *target) {
    return html_parse_target(target, session_id_, sizeof(session_id_),
                             target_path_, sizeof(target_path_));
  }

  void test(const char *target, const std::string_view &session_id,
            const std::string_view &normal_path) {
    BOOST_REQUIRE(parse(target));
    BOOST_TEST(session_id == session_id_);
    BOOST_TEST(normal_path == target_path_);
  }
};

BOOST_FIXTURE_TEST_CASE(RootDirNotFound, f) { BOOST_TEST(!parse("/")); }

BOOST_FIXTURE_TEST_CASE(BasicCaseSessionIdAndTarget, f) {
  test("sid/foo/bar.txt", "sid", "/foo/bar.txt");
  test("/sid/foo/bar.txt", "sid", "/foo/bar.txt");
}

BOOST_FIXTURE_TEST_CASE(AppendsIndexToDir, f) {
  test("session_id/", "session_id", "/index.html");
  test("session_id", "session_id", "/index.html");
  test("/session_id/", "session_id", "/index.html");
  test("/session_id", "session_id", "/index.html");
  test("/session_id/bar/", "session_id", "/bar/index.html");
  test("/session_id/bar", "session_id", "/bar");
  test("session_id/bar/", "session_id", "/bar/index.html");
}

BOOST_FIXTURE_TEST_CASE(MultipleSlashesAreNormalized, f) {
  test("///sid//foo/////bar.txt", "sid", "/foo/bar.txt");
}

BOOST_FIXTURE_TEST_CASE(CurrentDirectoryNormalizedOut, f) {
  test("/sid/././foo/././././bar.txt", "sid", "/foo/bar.txt");
  test("/sid/././foo/./././.", "sid", "/foo/index.html");
}

BOOST_FIXTURE_TEST_CASE(HiddenFilesNotFound, f) {
  BOOST_TEST(!parse("/sid/.foo"));
  BOOST_TEST(!parse("/sid/..foo"));
  BOOST_TEST(!parse("/sid/...foo"));
  BOOST_TEST(!parse("/sid/bar/baz/.foo"));
}

BOOST_FIXTURE_TEST_CASE(ParentDirectoryNormalizedOut, f) {
  test("/sid/foo/../bar.txt", "sid", "/bar.txt");
  test("/sid/foo/../../../bar.txt", "sid", "/bar.txt");
  test("/sid/../../../bar.txt", "sid", "/bar.txt");
  test("/sid/../bar.txt/..", "sid", "/index.html");
  test("/sid/../bar.txt/../", "sid", "/index.html");
}

BOOST_FIXTURE_TEST_CASE(MoreDotsInFileNameNotFound, f) {
  BOOST_TEST(!parse("/sid/..."));
  BOOST_TEST(!parse("/sid/.../"));
  BOOST_TEST(!parse("/sid/foo/..../../"));
}