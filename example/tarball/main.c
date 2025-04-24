#include <html_forms.h>

#include <stdio.h>
#include <string.h>

extern const char *tarball_path;

int main() {
  html_connection *con;

  if (!html_connect(&con)) {
    fprintf(stderr, "Failed to make html connection: %s\n", html_errmsg(con));
    return 1;
  }

  if (!html_upload_archive(con, "/", tarball_path)) {
    fprintf(stderr, "Failed to upload docroot archive: %s\n", html_errmsg(con));
    goto fail;
  }

  if (!html_navigate(con, "/index.html")) {
    fprintf(stderr, "Failed to navigate to /index.html: %s\n",
            html_errmsg(con));
    goto fail;
  }

  html_form *form;
  if (html_form_read(con, &form))
    html_form_free(form);
  html_disconnect(con);
  return 0;

fail:
  html_disconnect(con);
  return 1;
}
