#include <html-forms.h>

#include <stdio.h>
#include <string.h>

int main() {
  html_connection *con;

  if (!html_connect(&con)) {
    fprintf(stderr, "Failed to make html connection: %s\n", html_errmsg(con));
    return 1;
  }

  if (!html_upload_archive(con, "/", TARBALL_PATH)) {
    fprintf(stderr, "Failed to upload docroot archive: %s\n", html_errmsg(con));
    return 1;
  }

  if (!html_navigate(con, "/index.html")) {
    fprintf(stderr, "Failed to navigate to /index.html: %s\n",
            html_errmsg(con));
    return 1;
  }

  html_form form = NULL;
  html_read_form(con, &form);
  html_form_release(&form);
  return 0;
}
