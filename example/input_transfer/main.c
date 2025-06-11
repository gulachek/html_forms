/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include <html_forms.h>
#include <stdio.h>

extern const char *docroot;

int main() {
  html_connection *con;

  if (!html_connect(&con)) {
    fprintf(stderr, "Failed to connect to html forms server\n");
    return 1;
  }

  if (!html_upload_dir(con, "/", docroot)) {
    fprintf(stderr, "Failed to upload docroot: %s\n", html_errmsg(con));
    return 1;
  }

  if (!html_navigate(con, "/index.html")) {
    fprintf(stderr, "Failed to navigate to /index.html: %s\n",
            html_errmsg(con));
    return 1;
  }

  html_form *form;
  if (!html_form_read(con, &form)) {
    if (html_close_requested(con)) {
      return 0;
    } else {
      fprintf(stderr, "Failed to read form: %s\n", html_errmsg(con));
      return 1;
    }
  }

  html_disconnect(con);
  return 0;
}
