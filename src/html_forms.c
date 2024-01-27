#include "html_forms.h"
#include "html_connection.h"
#include "html_forms/encoding.h"
#include <msgstream.h>

#include <catui.h>
#include <cjson/cJSON.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include <dirent.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * https://www.php.net/manual/en/function.htmlspecialchars.php
 *
 * & (ampersand)	&amp;
 * " (double quote)	&quot;, unless ENT_NOQUOTES is set
 * ' (single quote)	&#039; (for ENT_HTML401) or &apos; (for ENT_XML1,
 * ENT_XHTML or ENT_HTML5), but only when ENT_QUOTES is set
 * < (less than)
 * &lt;
 * > (greater than)	&gt;
 */
size_t html_escape_size(const char *src) {
  if (!src)
    return 1; // null terminator

  size_t out = 0;
  for (size_t i = 0; src[i] != '\0'; ++i) {
    switch (src[i]) {
    case '&':
      out += 5;
      break;
    case '"':
    case '\'':
      out += 6;
      break;
    case '<':
    case '>':
      out += 4;
      break;
    default:
      out += 1;
    }
  }

  return out + 1; // +1 for null terminator
}

size_t html_escape(char *dst, size_t dst_size, const char *src) {
  size_t escape_len = html_escape_size(src);
  if (escape_len > dst_size)
    return escape_len;

  if (!src) {
    dst[0] = '\0';
    return 1;
  }

  size_t di = 0;
  for (size_t si = 0; src[si] != '\0'; ++si) {
    switch (src[si]) {
    case '&':
      memcpy(&dst[di], "&amp;", 5);
      di += 5;
      break;
    case '"':
      memcpy(&dst[di], "&quot;", 6);
      di += 6;
      break;
    case '\'':
      memcpy(&dst[di], "&#039;", 6);
      di += 6;
      break;
    case '<':
      memcpy(&dst[di], "&lt;", 4);
      di += 4;
      break;
    case '>':
      memcpy(&dst[di], "&gt;", 4);
      di += 4;
      break;
    default:
      dst[di] = src[si];
      di += 1;
    }
  }

  dst[di] = '\0';
  return escape_len;
}

struct html_mime_map_ {
  cJSON *array;
};

int html_connect(html_connection **pcon) {
  if (!pcon)
    goto fail;

  html_connection *con = *pcon = malloc(sizeof(struct html_connection_));
  if (!con)
    goto fail;

  con->close_requested = 0;

  FILE *f = tmpfile();
  if (!f) {
    printf_err(con, "Failed to create tmpfile for catui");
    goto fail;
  }

  con->fd = catui_connect("com.gulachek.html-forms", "0.1.0", f);
  if (con->fd == -1) {

    fflush(f);
    rewind(f);
    char buf[sizeof(con->errbuf)];
    size_t nread = fread(buf, 1, sizeof(buf), f);
    printf_err(con, "Failed to create catui connection: %*s", nread, buf);
    goto fail;
  }

  fclose(f);
  return 1;

fail:
  if (f)
    fclose(f);
  return 0;
}

void html_disconnect(html_connection *con) {
  if (!con)
    return;

  uint8_t buf[HTML_MSG_SIZE];
  int n = html_encode_omsg_close(buf, sizeof(buf));
  if (n >= 0) {
    msgstream_fd_send(con->fd, buf, sizeof(buf), n);
  }

  close(con->fd);
  free(con);
}

int html_close_requested(const html_connection *con) {
  if (!con)
    return 0;
  return con->close_requested;
}

void html_reject_close(html_connection *con) {
  if (!con)
    return;
  con->close_requested = 1;
}

const char *html_errmsg(html_connection *con) {
  if (!con)
    return NULL;

  return con->errbuf;
}

int html_encode_omsg_upload(void *data, size_t size, const char *url,
                            size_t content_length,
                            enum html_resource_type type) {
  // url: string
  // size?: number (missing means stream sized-chunks)
  // resType: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_OMSG_UPLOAD))
    return -1;

  if (content_length > 0) {
    if (!cJSON_AddNumberToObject(obj, "size", content_length))
      return -1;
  }

  if (!cJSON_AddStringToObject(obj, "url", url))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "resType", type)) {
    return -1;
  }

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

static int html_send_upload(html_connection *con, const char *url,
                            const char *file_path,
                            enum html_resource_type type) {

  if (!con)
    return 0;

  int fd = con->fd;

  struct stat stats;
  if (stat(file_path, &stats) == -1) {
    printf_err(con, "stat('%s'): %s", file_path, strerror(errno));
    return 0;
  }

  char buf[HTML_MSG_SIZE];
  int n = html_encode_omsg_upload(buf, sizeof(buf), url, stats.st_size, type);
  if (n < 0) {
    printf_err(con, "Failed to serialize message (likely memory issue)");
    return 0;
  }

  int ec = msgstream_fd_send(fd, buf, sizeof(buf), n);
  if (ec) {
    printf_err(con, "Failed to send message: %s", msgstream_errstr(ec));
    return 0;
  }

  FILE *f = fopen(file_path, "r");
  if (!f) {
    printf_err(con, "fopen('%s'): %s", file_path, strerror(errno));
    return 0;
  }

  size_t nleft = stats.st_size;
  while (nleft) {
    size_t n_to_read = HTML_MSG_SIZE < nleft ? HTML_MSG_SIZE : nleft;
    size_t nread = fread(buf, 1, n_to_read, f);
    if (nread == 0) {
      printf_err(con, "Read fewer bytes than expected on '%s': %s", file_path,
                 strerror(errno));
      return 0;
    }

    nleft -= nread;

    if (write(fd, buf, nread) == -1) {
      printf_err(con, "Failed to write file contents to connection: %s",
                 strerror(errno));
      return 0;
    }
  }

  return 1;
}

int html_upload_stream_open(html_connection *con, const char *url) {
  if (!con)
    return 0;

  char buf[HTML_MSG_SIZE];
  int n = html_encode_omsg_upload(buf, sizeof(buf), url, 0, HTML_RT_FILE);

  if (n < 0) {
    printf_err(con, "Failed to serialize message (likely memory issue)");
    return 0;
  }

  int ec = msgstream_fd_send(con->fd, buf, sizeof(buf), n);
  if (ec) {
    printf_err(con, "Failed to send message: %s", msgstream_errstr(ec));
    return 0;
  }

  return 1;
}

typedef uint8_t le16_buf[2];

static int le_encode(size_t n, le16_buf buf) {
  if (n > 0xffff)
    return 0;

  buf[0] = n % 0x100;
  n /= 0x100;
  buf[1] = n % 0x100;
  return 1;
}

static int le_decode(const le16_buf buf, size_t *n) {
  *n = buf[0] + (0x100 * buf[1]);
  return 1;
}

int html_upload_stream_write(html_connection *con, const void *data,
                             size_t size) {
  le16_buf chunk_size;
  if (!le_encode(size, chunk_size)) {
    printf_err(con, "Failed to encode chunk size header");
    return 0;
  }

  int ret = write(con->fd, chunk_size, sizeof(chunk_size));
  if (ret == -1) {
    printf_err(con, "Failed to write chunk size: %s", strerror(errno));
    return 0;
  }

  ret = write(con->fd, data, size);
  if (ret == -1) {
    printf_err(con, "Failed to write chunk: %s", strerror(errno));
    return 0;
  }

  return 1;
}

int html_upload_stream_close(html_connection *con) {
  // no current need to flush since unbuffered right now
  uint16_t zero = 0;
  int ret = write(con->fd, &zero, 2);
  if (ret == -1) {
    printf_err(con, "Failed to close stream: %s", strerror(errno));
    return 0;
  }

  return 1;
}

int html_upload_file(html_connection *con, const char *url,
                     const char *file_path) {
  return html_send_upload(con, url, file_path, HTML_RT_FILE);
}

int html_upload_archive(html_connection *con, const char *url,
                        const char *archive_path) {
  return html_send_upload(con, url, archive_path, HTML_RT_ARCHIVE);
}

int html_upload_dir(html_connection *con, const char *url,
                    const char *dir_path) {
  if (!con)
    return 0;

  if (!url) {
    printf_err(con, "null url arg");
    return 0;
  }

  if (!dir_path) {
    printf_err(con, "null dir_path arg");
    return 0;
  }

  DIR *dir = opendir(dir_path);
  if (!dir) {
    printf_err(con, "opendir('%s'): %s", dir_path, strerror(errno));
    return 0;
  }

  char sub_path[PATH_MAX];
  char sub_url[HTML_URL_SIZE];
#define PN sizeof(sub_path)
#define UN sizeof(sub_url)

  size_t base_path_len = strlcpy(sub_path, dir_path, PN);
  if (base_path_len >= PN) {
    printf_err(con, "Failed to copy dir path %s", dir_path);
    return 0;
  }

  if (sub_path[base_path_len - 1] != '/') {
    base_path_len = strlcat(sub_path, "/", PN);
    if (base_path_len >= PN) {
      printf_err(con, "Failed to append '/' to %s", sub_path);
      return 0;
    }
  }

  size_t base_url_len = strlcpy(sub_url, url, UN);
  if (base_url_len >= UN) {
    printf_err(con, "Failed to copy url %s", url);
    return 0;
  }

  if (sub_url[base_url_len - 1] != '/') {
    base_url_len = strlcat(sub_url, "/", UN);
    if (base_url_len >= UN) {
      printf_err(con, "Failed to append '/' to %s", sub_url);
      return 0;
    }
  }

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    // only interested in regular files and dirs
    switch (entry->d_type) {
    case DT_REG:
    case DT_DIR:
      break;
    default:
      continue;
    }

    if (entry->d_namlen < 1)
      continue;

    // ignore hidden files
    if (entry->d_name[0] == '.')
      continue;

    if (base_path_len + entry->d_namlen + 1 > PN) {
      printf_err(con, "No space to concatenate '%*s' to path '%s'",
                 entry->d_namlen, entry->d_name, sub_path);
      goto fail;
    }

    memcpy(sub_path + base_path_len, entry->d_name, entry->d_namlen);
    sub_path[base_path_len + entry->d_namlen] = '\0';

    if (base_url_len + entry->d_namlen + 1 > UN) {
      printf_err(con, "No space to concatenate '%*s' to url '%s'",
                 entry->d_namlen, entry->d_name, sub_url);
      goto fail;
    }

    memcpy(sub_url + base_url_len, entry->d_name, entry->d_namlen);
    sub_url[base_url_len + entry->d_namlen] = '\0';

    if (entry->d_type == DT_REG) {
      if (!html_upload_file(con, sub_url, sub_path)) {
        goto fail;
      }
    } else if (entry->d_type == DT_DIR) {
      if (!html_upload_dir(con, sub_url, sub_path)) {
        goto fail;
      }
    }
  }

#undef PN
#undef UN

  closedir(dir);
  return 1;

fail:
  closedir(dir);
  return 0;
}

int html_encode_omsg_navigate(void *data, size_t size, const char *url) {
  // url: string

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_OMSG_NAVIGATE))
    return -1;

  if (!cJSON_AddStringToObject(obj, "url", url))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_encode_omsg_close(void *data, size_t size) {
  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_OMSG_CLOSE))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_navigate(html_connection *con, const char *url) {
  if (!con)
    return 0;

  char buf[HTML_MSG_SIZE];
  int n = html_encode_omsg_navigate(buf, sizeof(buf), url);
  if (n < 0) {
    printf_err(con,
               "Failed to serialize navigate message (likely memory issue)");
    return 0;
  }
  int ec = msgstream_fd_send(con->fd, buf, sizeof(buf), n);
  if (ec) {
    printf_err(con, "Failed to send navigate message: %s",
               msgstream_errstr(ec));
    return 0;
  }

  return 1;
}

int html_encode_omsg_app_msg(void *data, size_t size, size_t content_length) {
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_OMSG_APP_MSG))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_send(html_connection *con, const void *data, size_t size) {
  if (!con)
    return 0;

  int fd = con->fd;

  char buf[HTML_MSG_SIZE];
  int n = html_encode_omsg_app_msg(buf, sizeof(buf), size);
  if (n < 0) {
    printf_err(con, "Failed to serialize message (likely memory issue)");
    return 0;
  }

  int ec = msgstream_fd_send(fd, buf, sizeof(buf), n);
  if (ec) {
    printf_err(con, "Failed to send message: %s", msgstream_errstr(ec));
    return 0;
  }

  if (write(fd, data, size) == -1) {
    printf_err(con, "Failed to write contents of message: %s", strerror(errno));
    return 0;
  }

  return 1;
}

static int copy_string(cJSON *obj, const char *prop, char *out,
                       size_t out_size) {
  cJSON *str = cJSON_GetObjectItem(obj, prop);
  if (!str)
    return 0;

  const char *val = cJSON_GetStringValue(str);
  if (!val)
    return 0;

  size_t n = strlcpy(out, val, out_size);
  return n <= out_size;
}

static int intval(cJSON *obj, const char *key, int *val) {
  cJSON *item = cJSON_GetObjectItem(obj, key);
  if (!(item && cJSON_IsNumber(item)))
    return 0;

  double dval = cJSON_GetNumberValue(item);
  int ival = (int)dval;
  if (ival != dval)
    return 0;

  *val = ival;
  return 1;
}

static int uintval(cJSON *obj, const char *key, unsigned int *val) {
  int ival;
  if (!intval(obj, key, &ival))
    return 0;

  if (ival < 0)
    return 0;

  *val = (unsigned int)ival;
  return 1;
}

static int html_decode_upload_msg(cJSON *obj, struct html_omsg_upload *msg) {
  // url: string
  // size?: number
  // archive: bool
  if (!copy_string(obj, "url", msg->url, sizeof(msg->url)))
    return 0;

  unsigned int size;
  if (cJSON_HasObjectItem(obj, "size")) {
    if (!uintval(obj, "size", &size))
      return 0;
  } else {
    size = 0;
  }
  msg->content_length = size;

  unsigned int rtype;
  if (!uintval(obj, "resType", &rtype))
    return 0;

  if (rtype > HTML_RT_ARCHIVE)
    return 0;

  msg->rtype = (enum html_resource_type)rtype;
  return 1;
}

static int html_decode_navigate_msg(cJSON *obj,
                                    struct html_omsg_navigate *msg) {
  if (!copy_string(obj, "url", msg->url, sizeof(msg->url)))
    return 0;

  return 1;
}

static int html_decode_app_msg(cJSON *obj, struct html_omsg_app_msg *msg) {
  cJSON *size = cJSON_GetObjectItem(obj, "size");
  if (!(size && cJSON_IsNumber(size)))
    return 0;
  double size_val = cJSON_GetNumberValue(size);
  if (size_val < 0)
    return 0;
  msg->content_length = (size_t)size_val;
  if (msg->content_length != size_val)
    return 0;

  return 1;
}

static int html_decode_mime_msg(cJSON *obj, html_mime_map *mimes) {
  if (!mimes)
    return 0;

  cJSON *map = cJSON_DetachItemFromObjectCaseSensitive(obj, "map");
  if (!cJSON_IsArray(map)) {
    goto fail;
  }

  cJSON *item;
  cJSON_ArrayForEach(item, map) {
    // [extname, mime]
    if (cJSON_GetArraySize(item) != 2)
      goto fail;

    cJSON *ext = cJSON_GetArrayItem(item, 0);
    if (!cJSON_IsString(ext))
      goto fail;

    cJSON *mime = cJSON_GetArrayItem(item, 1);
    if (!cJSON_IsString(ext))
      goto fail;
  }

  cJSON_Delete(mimes->array);
  mimes->array = map;
  return 1;

fail:
  cJSON_Delete(map);
  return 0;
}

int html_decode_out_msg(const void *data, size_t size,
                        struct html_out_msg *msg) {
  // TODO error message for failure conditions

  cJSON *obj = cJSON_ParseWithLength((const char *)data, size);
  if (!obj) {
    goto fail;
  }

  cJSON *type = cJSON_GetObjectItem(obj, "type");
  if (!(type && cJSON_IsNumber(type))) {
    goto fail;
  }

  int ret = 0;
  double type_val = cJSON_GetNumberValue(type);

  if (type_val == HTML_OMSG_UPLOAD) {
    msg->type = HTML_OMSG_UPLOAD;
    ret = html_decode_upload_msg(obj, &msg->msg.upload);
  } else if (type_val == HTML_OMSG_NAVIGATE) {
    msg->type = HTML_OMSG_NAVIGATE;
    ret = html_decode_navigate_msg(obj, &msg->msg.navigate);
  } else if (type_val == HTML_OMSG_APP_MSG) {
    msg->type = HTML_OMSG_APP_MSG;
    ret = html_decode_app_msg(obj, &msg->msg.app_msg);
  } else if (type_val == HTML_OMSG_MIME_MAP) {
    msg->type = HTML_OMSG_MIME_MAP;
    msg->msg.mime = html_mime_map_create();
    ret = html_decode_mime_msg(obj, msg->msg.mime);
  } else if (type_val == HTML_OMSG_CLOSE) {
    msg->type = HTML_OMSG_CLOSE;
    ret = 1;
  } else {
    goto fail;
  }

  cJSON_Delete(obj);
  return ret;

fail:
  cJSON_Delete(obj);
  return 0;
}

int html_encode_imsg_form(void *data, size_t size, size_t content_length,
                          const char *mime_type) {
  // mime: string
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_IMSG_FORM))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return -1;

  if (!cJSON_AddStringToObject(obj, "mime", mime_type))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_encode_imsg_error(void *data, size_t size, const char *msg) {
  // msg: string

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_IMSG_ERROR))
    return -1;

  if (!cJSON_AddStringToObject(obj, "msg", msg))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

static int html_decode_submit_form_msg(cJSON *obj, struct html_imsg_form *msg) {
  // mime: string
  // size: number
  if (!copy_string(obj, "mime", msg->mime_type, sizeof(msg->mime_type)))
    return 0;

  cJSON *size = cJSON_GetObjectItem(obj, "size");
  if (!(size && cJSON_IsNumber(size)))
    return 0;
  double size_val = cJSON_GetNumberValue(size);
  if (size_val < 0)
    return 0;
  msg->content_length = (size_t)size_val;
  if (msg->content_length != size_val)
    return 0;

  return 1;
}

static int html_decode_error(cJSON *obj, struct html_imsg_error *msg) {
  // msg: string
  if (!copy_string(obj, "msg", msg->msg, sizeof(msg->msg)))
    return 0;

  return 1;
}

int html_encode_imsg_app_msg(void *data, size_t size, size_t content_length) {
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_IMSG_APP_MSG))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_encode_imsg_close_req(void *data, size_t size) {
  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_IMSG_CLOSE_REQ))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

static int html_decode_recv_app_msg(cJSON *obj, struct html_imsg_app_msg *msg) {
  // size: number
  // TODO - factor this size calc out
  cJSON *size = cJSON_GetObjectItem(obj, "size");
  if (!(size && cJSON_IsNumber(size)))
    return 0;
  double size_val = cJSON_GetNumberValue(size);
  if (size_val < 0)
    return 0;
  msg->content_length = (size_t)size_val;
  if (msg->content_length != size_val)
    return 0;

  return 1;
}

int html_decode_in_msg(const void *data, size_t size, struct html_in_msg *msg) {
  // TODO error message for failure conditions

  cJSON *obj = cJSON_ParseWithLength((const char *)data, size);
  if (!obj) {
    goto fail;
  }

  cJSON *type = cJSON_GetObjectItem(obj, "type");
  if (!(type && cJSON_IsNumber(type))) {
    goto fail;
  }

  int ret = 0;
  double type_val = cJSON_GetNumberValue(type);

  if (type_val == HTML_IMSG_FORM) {
    msg->type = HTML_IMSG_FORM;
    ret = html_decode_submit_form_msg(obj, &msg->msg.form);
  } else if (type_val == HTML_IMSG_APP_MSG) {
    msg->type = HTML_IMSG_APP_MSG;
    ret = html_decode_recv_app_msg(obj, &msg->msg.app_msg);
  } else if (type_val == HTML_IMSG_CLOSE_REQ) {
    msg->type = HTML_IMSG_CLOSE_REQ;
    ret = 1;
  } else if (type_val == HTML_IMSG_ERROR) {
    msg->type = HTML_IMSG_ERROR;
    ret = html_decode_error(obj, &msg->msg.error);
  } else {
    goto fail;
  }

  cJSON_Delete(obj);
  return ret;

fail:
  cJSON_Delete(obj);
  return 0;
}

static int read_msg_type(html_connection *con, struct html_in_msg *msg,
                         int msg_type) {
  uint8_t buf[HTML_MSG_SIZE];
  size_t n;
  int ec = msgstream_fd_recv(con->fd, buf, sizeof(buf), &n);
  if (ec) {
    printf_err(con, "Failed to receive input message: %s",
               msgstream_errstr(ec));
    return 0;
  }

  if (!html_decode_in_msg(buf, n, msg)) {
    printf_err(con, "Failed to parse input message");
    return 0;
  }

  if (msg->type != msg_type) {
    if (msg->type == HTML_IMSG_CLOSE_REQ) {
      printf_err(con, "Close requested by user");
      con->close_requested = 1;
    } else if (msg->type == HTML_IMSG_ERROR) {
      printf_err(con, "(server): %s", msg->msg.error.msg);
    } else {
      printf_err(con, "Unexpected message type: %d", msg->type);
    }

    return 0;
  }

  return 1;
}

static int readn(html_connection *con, size_t n, void *data) {
  size_t nread = 0;
  while (nread < n) {
    ssize_t ret = read(con->fd, data + nread, n - nread);
    if (ret < 1) {
      printf_err(con, "read() failed: %s", strerror(errno));
      return 0;
    }

    nread += ret;
  }

  return 1;
}

static int html_read_form_data(html_connection *con, void *data, size_t size,
                               size_t *pnread) {
  struct html_in_msg msg;
  if (!read_msg_type(con, &msg, HTML_IMSG_FORM))
    return 0;

  const char *x_www_form_urlencoded = "application/x-www-form-urlencoded";
  struct html_imsg_form *form = &msg.msg.form;
  if (strcmp(form->mime_type, x_www_form_urlencoded) != 0) {
    printf_err(con, "Unexpected form mime type '%s' (expected '%s')",
               form->mime_type, x_www_form_urlencoded);
    return 0;
  }

  if (form->content_length + 1 > size) {
    printf_err(con,
               "Form buffer of size %lu is too small for received form of size "
               "%lu (plus null terminator)",
               size, form->content_length);
    return 0;
  }

  if (!readn(con, form->content_length, data))
    return 0;

  ((char *)data)[form->content_length] = '\0';
  *pnread = form->content_length;
  return 1;
}

int html_recv(html_connection *con, void *data, size_t size, size_t *msg_size) {
  if (!con)
    return 0;

  if (!msg_size) {
    printf_err(con, "null 'msg_size' argument");
    return 0;
  }

  struct html_in_msg msg;
  if (!read_msg_type(con, &msg, HTML_IMSG_APP_MSG))
    return 0;

  struct html_imsg_app_msg *app_msg = &msg.msg.app_msg;
  if (app_msg->content_length > size) {
    printf_err(con,
               "Buffer of size %lu is too small for message of "
               "size %lu",
               size, app_msg->content_length);
    return 0;
  }

  if (!readn(con, app_msg->content_length, data))
    return 0;

  *msg_size = app_msg->content_length;
  return 1;
}

struct html_form_field {
  char *name;
  char *value;
};

struct html_form_ {
  size_t size;
  struct html_form_field *fields;
};

static int is_valid_pct_char(const char *buf, int size, int offset) {
  return (size - offset >= 3) && (buf[offset] == '%') &&
         ishexnumber(buf[offset + 1]) && ishexnumber(buf[offset + 2]);
}

static int hexval(char c) {
  if (isdigit(c))
    return c - '0';

  if (islower(c))
    return (c - 'a') + 10;

  if (isupper(c))
    return (c - 'A') + 10;

  return -1;
}

// ASSUMES AT LEAST 2 BYTES
static char hexbyte(const char *str) {
  union {
    unsigned char u;
    char c;
  } val;
  val.u = (hexval(str[0]) << 4) | hexval(str[1]);
  return val.c;
}

static char *percent_decode(const char *buf, size_t size) {
  int decoded_size = 0;
  for (int i = 0; i < size; ++i) {
    if (buf[i] == '%') {
      if (!is_valid_pct_char(buf, size, i))
        return NULL;

      i += 2;
    }

    decoded_size += 1;
  }

  char *decoded = malloc(decoded_size + 1);
  if (!decoded)
    return NULL;

  int di = 0;
  for (int i = 0; i < size; ++i) {
    if (buf[i] == '+') {
      decoded[di] = ' ';
    } else if (buf[i] == '%') {
      decoded[di] = hexbyte(buf + i + 1);
      i += 2;
    } else {
      decoded[di] = buf[i];
    }

    ++di;
  }

  decoded[decoded_size] = '\0';
  return decoded;
}

static int parse_field(char *buf, size_t size, int offset,
                       struct html_form_field *field) {
  int field_end = size, name_end = -1;
  for (int i = offset; i < size; ++i) {
    if (buf[i] == '=') {
      if (name_end == -1)
        name_end = i;
      else
        return -1; // unexpected to have multiple = in field
    } else if (buf[i] == '&') {
      field_end = i;
      break;
    }
  }

  int field_size = field_end - offset;
  int value_pct_size = field_end - name_end - 1; // magic 1 is for '='
  if (name_end == -1) {
    name_end = field_end;
    value_pct_size = 0;
  }

  int name_pct_size = name_end - offset;
  const char *name_start = buf + offset;
  const char *val_start = buf + field_end - value_pct_size;

  if (!(field->name = percent_decode(name_start, name_pct_size)))
    return -1;

  if (!(field->value = percent_decode(val_start, value_pct_size)))
    return -1;

  return field_size;
}

int html_form_read(html_connection *con, html_form **pform) {
  if (!con)
    return 0;

  if (!pform) {
    printf_err(con, "null 'pform' argument");
    return 0;
  }

  char buf[HTML_FORM_SIZE];
  size_t n;
  if (!html_read_form_data(con, buf, sizeof(buf), &n))
    return 0;

  int nfields = n > 0 ? 1 : 0;
  for (int i = 0; i < n; ++i) {
    if (buf[i] == '&')
      ++nfields;
  }

  html_form *form;
  if ((form = malloc(sizeof(html_form))) == NULL) {
    printf_err(con, "Failed to allocate html_form struct");
    return 0;
  }

  form->size = nfields;
  if ((form->fields = calloc(nfields, sizeof(struct html_form_field))) ==
      NULL) {
    printf_err(con, "Failed to allocate form for %d fields", nfields);
    return 0;
  }

  int i = 0;
  for (int field_i = 0; field_i < nfields; ++field_i) {
    int field_size = parse_field(buf, n, i, &form->fields[field_i]);
    if (field_size < 0) {
      printf_err(con,
                 "Failed to parse form field %d in '%s' (starting "
                 "at '%5s')",
                 field_i, buf, &buf[i]);
      return 0;
    }

    i += field_size + 1; // magic 1 for '&'
  }

  *pform = form;
  return 1;
}

void html_form_free(html_form *form) {
  if (!form)
    return;

  for (size_t i = 0; i < form->size; ++i) {
    struct html_form_field *field = &form->fields[i];
    free(field->name);
    free(field->value);
  }

  free(form->fields);
  free(form);
}

size_t html_form_size(const html_form *form) {
  if (!form)
    return 0;
  return form->size;
}

const char *html_form_name_at(const html_form *form, size_t i) {
  if (!(form && i < form->size))
    return NULL;
  return form->fields[i].name;
}

const char *html_form_value_at(const html_form *form, size_t i) {
  if (!(form && i < form->size))
    return NULL;
  return form->fields[i].value;
}

const char *html_form_value_of(const html_form *form, const char *field_name) {
  if (!form)
    return NULL;

  for (size_t i = 0; i < form->size; ++i) {
    if (strcmp(field_name, form->fields[i].name) == 0)
      return form->fields[i].value;
  }

  return NULL;
}

html_mime_map *html_mime_map_create() {
  html_mime_map *ptr = malloc(sizeof(html_mime_map));
  if (!ptr)
    return NULL;

  cJSON *array = cJSON_CreateArray();
  if (!array) {
    free(ptr);
    return NULL;
  }

  ptr->array = array;
  return ptr;
}

void html_mime_map_free(html_mime_map *mimes) {
  if (!mimes)
    return;

  cJSON_Delete(mimes->array);
  free(mimes);
}

int html_mime_map_add(html_mime_map *mimes, const char *extname,
                      const char *mime_type) {
  if (!(mimes && extname && mime_type))
    return 0;

  size_t extlen = strlen(extname);
  size_t mimelen = strlen(mime_type);

  if (extlen < 1 || extlen > 16 || mimelen < 1 || mimelen > HTML_MIME_SIZE)
    return 0;

  cJSON *item = cJSON_CreateArray();
  if (!item)
    goto fail;

  size_t offset = extname[0] == '.' ? 1 : 0;
  cJSON *ext = cJSON_CreateString(extname + offset);
  if (!ext)
    goto fail;

  if (!cJSON_AddItemToArray(item, ext)) {
    cJSON_Delete(ext);
    goto fail;
  }

  cJSON *mime = cJSON_CreateString(mime_type);
  if (!mime)
    goto fail;

  if (!cJSON_AddItemToArray(item, mime)) {
    cJSON_Delete(mime);
    goto fail;
  }

  if (!cJSON_AddItemToArray(mimes->array, item)) {
    goto fail;
  }

  return 1;

fail:
  if (item)
    cJSON_Delete(item);
  return 0;
}

int html_encode_omsg_mime_map(void *data, size_t size,
                              const html_mime_map *mimes) {
  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_OMSG_MIME_MAP))
    return -1;

  if (!cJSON_AddItemReferenceToObject(obj, "map", mimes->array)) {
    return -1;
  }

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_mime_map_apply(html_connection *con, const html_mime_map *mimes) {
  if (!con)
    return 0;

  if (!mimes) {
    printf_err(con, "null 'mimes' arg");
    return 0;
  }

  char buf[HTML_MSG_SIZE];
  int n = html_encode_omsg_mime_map(buf, sizeof(buf), mimes);
  if (n < 0) {
    printf_err(con, "Failed to encode mime map message (usually "
                    "memory constraint)");
    return 0;
  }

  int ec = msgstream_fd_send(con->fd, buf, sizeof(buf), n);
  if (ec) {
    printf_err(con, "Failed to send mime message: %s", msgstream_errstr(ec));
    return 0;
  }

  return 1;
}

size_t html_mime_map_size(const html_mime_map *mimes) {
  if (!mimes)
    return 0;
  return cJSON_GetArraySize(mimes->array);
}

int html_mime_map_entry_at(const html_mime_map *mimes, size_t i,
                           const char **extname, const char **mime_type) {
  if (!mimes)
    return 0;

  if (i >= cJSON_GetArraySize(mimes->array))
    return 0;

  cJSON *item = cJSON_GetArrayItem(mimes->array, i);

  cJSON *ext_str = cJSON_GetArrayItem(item, 0);
  *extname = cJSON_GetStringValue(ext_str);

  cJSON *mime_str = cJSON_GetArrayItem(item, 1);
  *mime_type = cJSON_GetStringValue(mime_str);

  return 1;
}

int printf_err(html_connection *con, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = vsnprintf(con->errbuf, sizeof(con->errbuf), fmt, args);
  va_end(args);
  return ret;
}
