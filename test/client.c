#include <html-forms.h>

#include <catui.h>
#include <msgstream.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

int upload_files(int fd);

int main() {
  int fd = html_connect(stderr);

  if (fd == -1)
    return 1;

  if (!upload_files(fd))
    return 1;

  if (!html_navigate(fd, "/index.html"))
    return 1;

  html_form form;
  if (html_read_form(fd, &form) < 0) {
    return 1;
  }

  printf("Response: %s\n", html_form_value_of(form, "response"));

  if (!html_navigate(fd, "/other.html")) {
    return 1;
  }

  // TODO - should auto release
  html_form_release(&form);

  if (html_read_form(fd, &form) < 0) {
    return 1;
  }

  printf("Response: %s\n", html_form_value_of(form, "action"));

  return 0;
}

int upload_files(int fd) {
  if (!html_upload(fd, "/index.html", "./test/index.html", "text/html"))
    return 0;

  if (!html_upload(fd, "/index.css", "./test/index.css", "text/css")) {
    return 0;
  }

  if (!html_upload(fd, "/favicon.ico", "./test/favicon.ico", "image/x-icon"))
    return 0;

  if (!html_upload(fd, "/other.html", "./test/other.html", "text/html"))
    return 0;

  return 1;
}
