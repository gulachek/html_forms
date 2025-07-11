/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <cassert>
#include <cstring>
#include <html_forms_server.h>
#include <html_forms_server/private/evt_util.hpp>

void copy_session_id(const std::string &s, char *session_id_buf) {
  if (s.length() >= HTML_FORMS_SERVER_SESSION_ID_SIZE) {
    // Malformed program if this happens
    assert(false);
    session_id_buf[0] = '\0'; // truncate
    return;
  }

  std::strncpy(session_id_buf, s.c_str(), HTML_FORMS_SERVER_SESSION_ID_SIZE);
}
