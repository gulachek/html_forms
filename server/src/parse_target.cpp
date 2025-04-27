/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#include "html_forms_server/private/parse_target.hpp"
#include <cstring>

using std::size_t;

static int parse_dot_file(const char *target, size_t n, int offset,
                          int *dot_len, int *only_dots) {
  *dot_len = 0;
  *only_dots = 1;
  int i = offset;
  for (; i < n && target[i] != '/'; ++i) {
    if (target[i] == '.') {
      *dot_len += 1;
    } else {
      *only_dots = 0;
    }
  }

  return i;
}

static int rfind(const char *str, int offset, char search) {
  for (int i = offset; i >= 0; --i) {
    if (str[i] == search)
      return i;
  }

  return -1;
}

int parse_target(const char *target, char *session_id, size_t session_id_len,
                 char *normalized_path, size_t norm_path_len) {
  int n = std::strlen(target);

  if (session_id_len < 1 || norm_path_len < 1)
    return 0; // not useful at all

  // find session id (first non-empty piece of target)
  int start = 0, i = 0, session_id_n = 0;
  for (; i < n; ++i) {
    if (target[i] == '/') {
      if (session_id_n)
        break; // done
    } else {
      if (session_id_n >= session_id_len - 1) {
        return 0; // can't fit session id
      } else {
        session_id[session_id_n++] = target[i];
      }
    }
  }

  // session id not found
  if (session_id_n < 1)
    return 0;

  session_id[session_id_n] = '\0';

  // normalize path ...
  int norm_path_n = 0;
  while (i < n) {
    switch (target[i]) {
    case '@':
    case '%':
    case '+':
      return 0;
    default:
      break;
    }

    // virtual abs path
    if (target[i] == '~') {
      if (target[i - 1] == '/')
        norm_path_n = 1;
      else
        return 0;

      if ((i + 1) < n && target[i + 1] != '/')
        return 0;

      ++i;
      continue;
    }

    // deal with '.+' directories
    if (target[i] == '.' && target[i - 1] == '/') {
      int dot_len, only_dots;
      i = parse_dot_file(target, n, i, &dot_len, &only_dots);

      if (!only_dots)
        return 0; // hidden files not found

      if (dot_len == 1) {
        continue;
      } else if (dot_len == 2) {
        int current_dir_i = rfind(normalized_path, norm_path_n - 1, '/');
        int parent_dir_i = rfind(normalized_path, current_dir_i - 1, '/');
        if (parent_dir_i >= 0)
          norm_path_n = parent_dir_i + 1;

        continue;
      } else {
        return 0; // not handled
      }
    }

    if (norm_path_n >= norm_path_len - 1)
      return 0;

    // ignore multiple '/'
    if (!(target[i] == '/' && norm_path_n > 0 &&
          normalized_path[norm_path_n - 1] == '/')) {
      normalized_path[norm_path_n++] = target[i];
    }

    ++i;
  }

  if (norm_path_n < 1) {
    normalized_path[0] = '/';
    norm_path_n = 1;
  }

  if (normalized_path[norm_path_n - 1] == '/') {
    size_t remaining = norm_path_len - norm_path_n;
    size_t index_n =
        strlcpy(normalized_path + norm_path_n, "index.html", remaining);
    if (index_n > remaining)
      return 0; // couldn't fit

    norm_path_n += index_n;
  }

  normalized_path[norm_path_n] = '\0';
  return 1;
}
