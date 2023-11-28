#include "open-url.hpp"
#include <cstdlib>
#include <sstream>

void open_url(const std::string_view &url) {
#if PLATFORM_MACOS
  std::ostringstream cmd;
  cmd << "open " << url;
  std::system(cmd.str().c_str());
#else
#error NEED TO IMPLEMENT open_url
#endif
}
