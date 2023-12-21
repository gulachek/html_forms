#include <html-forms.h>

#include <stdio.h>
#include <string.h>

int override_mimes(html_connection con);

int main() {
  html_connection con = html_connection_alloc();
  if (!con) {
    fprintf(stderr, "Failed to allocate html connection\n");
    return 1;
  }

  if (!html_connect(con)) {
    // TODO - print error in connection
    return 1;
  }

  if (!override_mimes(con))
    return 1;

  if (!html_upload_dir(con, "/", "./example/mime_swap/docroot"))
    return 1;

  html_form form = NULL;

  if (!html_navigate(con, "/markup.css"))
    return 1;

  html_read_form(con, &form);
  html_form_release(&form);
  return 0;
}

int override_mimes(html_connection con) {
  html_mime_map mimes = html_mime_map_alloc();
  if (!mimes) {
    fprintf(stderr, "Failed to allocate mime map\n");
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

  if (!html_upload_mime_map(con, mimes)) {
    fprintf(stderr, "Failed to upload mime map\n");
    return 0;
  }

  html_mime_map_free(&mimes);
  return 1;
}