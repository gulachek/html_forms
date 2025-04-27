/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef HTML_CONNECTION_H
#define HTML_CONNECTION_H

#include "html_forms.h"

struct html_connection_ {
  int fd;
  int close_requested;
  char errbuf[512];
};

int printf_err(html_connection *con, const char *fmt, ...);

#endif
