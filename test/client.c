#include <html-forms.h>

#include <catui.h>
#include <msgstream.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

int upload_files(html_connection con);

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

  if (!upload_files(con))
    return 1;

  html_form form = NULL;

  while (1) {
    if (!html_navigate(con, "/index.html"))
      return 1;

    if (html_read_form(con, &form) < 0) {
      return 1;
    }

    const char *response = html_form_value_of(form, "response");
    if (strcmp(response, "quit") == 0)
      break;

    printf("Response: %s\n", response);

    if (!html_navigate(con, "/other.html")) {
      return 1;
    }

    char sync_buf[16];
    if (html_recv_js_message(con, sync_buf, sizeof(sync_buf)) < 0) {
      return 1;
    }

    if (!html_send_js_message(con, response))
      return 1;

    if (html_read_form(con, &form) < 0) {
      return 1;
    }

    const char *action = html_form_value_of(form, "action");
    if (strcmp(action, "home") != 0) {
      printf("Quitting with action '%s'\n", action);
      break;
    }
  }

  html_form_release(&form);
  return 0;
}

int upload_files(html_connection con) {
  if (!html_upload(con, "/index.html", "./test/index.html", "text/html"))
    return 0;

  if (!html_upload(con, "/index.css", "./test/index.css", "text/css")) {
    return 0;
  }

  if (!html_upload(con, "/favicon.ico", "./test/favicon.ico", "image/x-icon"))
    return 0;

  if (!html_upload(con, "/other.html", "./test/other.html", "text/html"))
    return 0;

  if (!html_upload(con, "/other.js", "./test/other.js", "text/javascript"))
    return 0;

  return 1;
}
