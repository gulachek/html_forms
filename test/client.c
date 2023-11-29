#include <html-forms.h>

#include <catui.h>
#include <msgstream.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

#define MSGSIZE 2048
#define URLSIZE 512
#define MIMELEN 128
#define PROMPT_SIZE 4096

// TODO - check off by one for '\0' at end of c string
int validate_snprintf(int ret, char **pbuf, size_t *buflen, FILE *err) {
  if (ret < 0) {
    fprintf(err, "error serializing message\n");
    return 0;
  }

  if (ret > *buflen - 1) {
    fprintf(err, "message too big\n");
    return 0;
  }

  *buflen -= ret;
  *pbuf += ret;
  return 1;
}

int send_message(int fd, struct html_msg *msg, FILE *err) {
  char buf_[MSGSIZE];
  char *buf = buf_;
  size_t buflen = MSGSIZE;

  int ret = snprintf(buf, buflen, "{\"type\":%d,", msg->type);
  if (!validate_snprintf(ret, &buf, &buflen, err)) {
    return 0;
  }

  if (msg->type == HTML_BEGIN_UPLOAD) {
    struct begin_upload *upload = &msg->msg.upload;
    // TODO - json encode strings
    ret = snprintf(buf, buflen, "\"size\":%lu,\"mime\":\"%s\",\"url\":\"%s\"}",
                   upload->content_length, upload->mime_type, upload->url);
    if (!validate_snprintf(ret, &buf, &buflen, err))
      return 0;
  } else if (msg->type == HTML_PROMPT) {
    struct prompt *prompt = &msg->msg.prompt;
    ret = snprintf(buf, buflen, "\"url\":\"%s\"}", prompt->url);
    if (!validate_snprintf(ret, &buf, &buflen, err))
      return 0;
  } else {
    fprintf(err, "Unknown message type %d\n", msg->type);
    return 0;
  }

  msgstream_size n = msgstream_send(fd, buf_, MSGSIZE, strlen(buf_), err);
  return n >= 0;
}

int upload_file(int fd, const char *url_path_abs, const char *file_path,
                const char *mime, FILE *err);

int prompt(int fd, const char *url_path_abs, char *prompt_buf, FILE *err);

int main() {
  // int fd = catui_connect("com.gulachek.html-forms", "0.1.0", stderr);
  int fd = html_connect(stderr);

  if (fd == -1)
    return 1;

  if (!upload_file(fd, "/index.html", "./test/index.html", "text/html",
                   stderr)) {
    return 1;
  }

  if (!upload_file(fd, "/index.css", "./test/index.css", "text/css", stderr)) {
    return 1;
  }

  if (!upload_file(fd, "/favicon.ico", "./test/favicon.ico", "image/x-icon",
                   stderr)) {
    return 1;
  }

  char buf[PROMPT_SIZE];
  if (!prompt(fd, "/index.html", buf, stderr)) {
    return 1;
  }

  printf("Read body: %s\n", buf);
  return 0;
}

void fperror(FILE *err, const char *s) {
  if (!err)
    return;
  const char *msg = strerror(errno);
  if (s)
    fprintf(err, "%s: %s\n", s, msg);
  else
    fprintf(err, "%s\n", msg);
}

int upload_file(int fd, const char *url_path_abs, const char *file_path,
                const char *mime, FILE *err) {
  struct stat stats;
  if (stat(file_path, &stats) == -1) {
    fperror(err, "stat");
    return 0;
  }

  struct html_msg msg;
  msg.type = HTML_BEGIN_UPLOAD;
  struct begin_upload *upload = &msg.msg.upload;
  strncpy(upload->mime_type, mime, MIMELEN);
  strncpy(upload->url, url_path_abs, URLSIZE);
  upload->content_length = stats.st_size;
  if (!send_message(fd, &msg, err)) {
    fprintf(err, "Failed to upload file '%s' (%s)\n", url_path_abs, file_path);
    return 0;
  }

  FILE *f = fopen(file_path, "r");
  if (!f) {
    fperror(err, "fopen");
    return 0;
  }

  size_t nread;
  char buf[4096];
  while ((nread = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (write(fd, buf, nread) == -1) {
      fperror(err, "write");
      return 0;
    }
  }

  if (ferror(f)) {
    fprintf(err, "Error reading file '%s' while uploading\n", file_path);
    return 0;
  }

  return 1;
}

int prompt(int fd, const char *url_path_abs, char *prompt_buf, FILE *err) {
  struct html_msg msg;
  msg.type = HTML_PROMPT;
  struct prompt *prompt = &msg.msg.prompt;
  strncpy(prompt->url, url_path_abs, URLSIZE);
  if (!send_message(fd, &msg, err)) {
    fprintf(err, "Failed to prompt url '%s'\n", url_path_abs);
    return -1;
  }

  msgstream_size nread = msgstream_recv(fd, prompt_buf, PROMPT_SIZE, stderr);
  if (nread >= PROMPT_SIZE) {
    fprintf(err, "Size too big\n");
    return -1;
  }

  prompt_buf[nread] = '\0';
  return nread;
}
