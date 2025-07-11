/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#ifndef HTML_FORMS_SERVER_PRIVATE_EVT_UTIL_HPP
#define HTML_FORMS_SERVER_PRIVATE_EVT_UTIL_HPP

#include <string>

/**
 * Copy string session id to c string buffer. Assumes at least
 * HTML_FORMS_SERVER_SESSION_ID_SIZE bytes available
 * @param[in] s The session ID to copy
 * @param[out] session_id_buf The buffer to copy the ID into
 * @remarks Bails if s is too long
 */
void copy_session_id(const std::string &s, char *session_id_buf);

#endif
