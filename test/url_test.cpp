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
    BOOST_TEST(parse(target));
    BOOST_TEST(session_id == session_id_);
    BOOST_TEST(normal_path == target_path_);
  }
};

BOOST_FIXTURE_TEST_CASE(RootDirNotFound, f) { BOOST_TEST(!parse("/")); }

BOOST_FIXTURE_TEST_CASE(SessionIdIsFirstPart, f) {
  test("/session_id", "session_id", "/index.html");
}
