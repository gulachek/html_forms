#include <html-forms.h>

#include <stdio.h>
#include <string.h>

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

  if (!html_upload_archive(con, "/", TARBALL_PATH))
    return 1;

  if (!html_navigate(con, "/index.html"))
    return 1;

  html_form form = NULL;
  html_read_form(con, &form);
  html_form_release(&form);
  return 0;
}