#include <html_forms.h>

#include <stdio.h>
#include <string.h>

int override_mimes(html_connection *con);

int main() {
  html_connection *con;

  if (!html_connect(&con)) {
    fprintf(stderr, "Failed to make html connection: %s\n", html_errmsg(con));
    return 1;
  }

  if (!override_mimes(con))
    goto fail;

  if (!html_upload_dir(con, "/", "./example/mime_swap/docroot")) {
    fprintf(stderr, "Failed to upload docroot: %s\n", html_errmsg(con));
    goto fail;
  }

  if (!html_navigate(con, "/markup.css")) {
    fprintf(stderr, "Failed to navigate to /markup.css: %s\n",
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

int override_mimes(html_connection *con) {
  html_mime_map *mimes = html_mime_map_create();
  if (!mimes) {
    fprintf(stderr, "Failed to create mime map\n");
    return 0;
  }

  if (!html_mime_map_add(mimes, ".css", "text/html")) {
    fprintf(stderr, "Failed to map .css -> text/html\n");
    return 0;
  }

  if (!html_mime_map_add(mimes, "html", "text/css")) {

    fprintf(stderr, "Failed to map .html -> text/css\n");
    return 0;
  }

  if (!html_mime_map_apply(con, mimes)) {
    fprintf(stderr, "Failed to apply mime map: %s\n", html_errmsg(con));
    return 0;
  }

  html_mime_map_free(mimes);
  return 1;
}
