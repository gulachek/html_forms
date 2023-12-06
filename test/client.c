#include <html-forms.h>

#include <catui.h>
#include <msgstream.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
  // int fd = catui_connect("com.gulachek.html-forms", "0.1.0", stderr);
  int fd = html_connect(stderr);

  if (fd == -1)
    return 1;

  if (!html_upload(fd, "/index.html", "./test/index.html", "text/html"))
    return 1;

  if (!html_upload(fd, "/index.css", "./test/index.css", "text/css")) {
    return 1;
  }

  if (!html_upload(fd, "/favicon.ico", "./test/favicon.ico", "image/x-icon")) {
    return 1;
  }

  if (!html_navigate(fd, "/index.html"))
    return 1;

  html_form_buf buf;
  if (html_read_form(fd, buf, sizeof(buf)) < 0) {
    return 1;
  }

  printf("Read form: %s\n", buf);

  if (!html_upload(fd, "/other.html", "./test/other.html", "text/html")) {
    return 1;
  }

  if (!html_navigate(fd, "/other.html")) {
    return 1;
  }

  if (html_read_form(fd, buf, sizeof(buf)) < 0) {
    return 1;
  }

  printf("Read form: %s\n", buf);

  return 0;
}
