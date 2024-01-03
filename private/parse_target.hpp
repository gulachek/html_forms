#ifndef PARSE_TARGET_HPP
#define PARSE_TARGET_HPP

#include <cstdlib>

int parse_target(const char *target, char *session_id,
                 std::size_t session_id_len, char *normalized_path,
                 std::size_t norm_path_len);

#endif
