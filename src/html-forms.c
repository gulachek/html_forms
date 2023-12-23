#include "html-forms.h"
#include "html_connection.h"
#include <msgstream.h>

#include <catui.h>
#include <cjson/cJSON.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <sys/dirent.h>
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
    return 0;

  html_connection *con = *pcon = malloc(sizeof(struct html_connection_));
  if (!con)
    return 0;

  con->fd = catui_connect("com.gulachek.html-forms", "0.1.0", stderr);
  if (con->fd == -1) {
    // TODO - add error to connection
    return 0;
  }

  return 1;
}

void html_disconnect(html_connection *con) {
  if (!con)
    return;

  free(con);
}

int html_encode_upload(void *data, size_t size, const char *url,
                       size_t content_length, int is_archive) {
  // url: string
  // size: number
  // archive: bool

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_BEGIN_UPLOAD))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return -1;

  if (!cJSON_AddStringToObject(obj, "url", url))
    return -1;

  if (!cJSON_AddBoolToObject(obj, "archive", !!is_archive)) {
    return -1;
  }

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

static int html_send_upload(html_connection *con, const char *url,
                            const char *file_path, int is_archive) {

  if (!con)
    return 0;

  int fd = con->fd;

  struct stat stats;
  if (stat(file_path, &stats) == -1) {
    return 0;
  }

  char buf[HTML_MSG_SIZE];
  int n = html_encode_upload(buf, sizeof(buf), url, stats.st_size, is_archive);
  if (n < 0)
    return n;

  if (msgstream_fd_send(fd, buf, sizeof(buf), n))
    return 0;

  FILE *f = fopen(file_path, "r");
  if (!f)
    return 0;

  size_t nleft = stats.st_size;
  while (nleft) {
    size_t n_to_read = HTML_MSG_SIZE < nleft ? HTML_MSG_SIZE : nleft;
    size_t nread = fread(buf, 1, n_to_read, f);
    if (nread == 0)
      return 0;

    nleft -= nread;

    if (write(fd, buf, nread) == -1) {
      perror("write");
      return 0;
    }
  }

  return 1;
}

int html_upload_file(html_connection *con, const char *url,
                     const char *file_path) {
  return html_send_upload(con, url, file_path, /* is_archive: */ 0);
}

int html_upload_archive(html_connection *con, const char *url,
                        const char *archive_path) {
  return html_send_upload(con, url, archive_path, /* is_archive: */ 1);
}

int html_upload_dir(html_connection *con, const char *url,
                    const char *dir_path) {
  if (!(con && url && dir_path))
    return 0;

  DIR *dir = opendir(dir_path);
  if (!dir)
    return 0;

  char sub_path[PATH_MAX];
  char sub_url[HTML_URL_SIZE];
#define PN sizeof(sub_path)
#define UN sizeof(sub_url)

  size_t base_path_len = strlcpy(sub_path, dir_path, PN);
  if (base_path_len >= PN)
    return 0;

  if (sub_path[base_path_len - 1] != '/') {
    base_path_len = strlcat(sub_path, "/", PN);
    if (base_path_len >= PN)
      return 0;
  }

  size_t base_url_len = strlcpy(sub_url, url, UN);
  if (base_url_len >= UN)
    return 0;

  if (sub_url[base_url_len - 1] != '/') {
    base_url_len = strlcat(sub_url, "/", UN);
    if (base_url_len >= UN)
      return 0;
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
      goto fail;
    }

    memcpy(sub_path + base_path_len, entry->d_name, entry->d_namlen);
    sub_path[base_path_len + entry->d_namlen] = '\0';

    if (base_url_len + entry->d_namlen + 1 > UN) {
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

int html_encode_navigate(void *data, size_t size, const char *url) {
  // url: string

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_NAVIGATE))
    return -1;

  if (!cJSON_AddStringToObject(obj, "url", url))
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
  int n = html_encode_navigate(buf, sizeof(buf), url);
  if (n < 0)
    return 0;

  if (msgstream_fd_send(con->fd, buf, sizeof(buf), n))
    return 0;

  return 1;
}

int html_encode_js_message(void *data, size_t size, size_t content_length) {
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_JS_MESSAGE))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

// TODO - return value to bool
int html_send_js_message(html_connection *con, const char *msg) {
  if (!con)
    return -1;

  int fd = con->fd;

  char buf[HTML_MSG_SIZE];
  size_t msg_size = strlen(msg);
  int n = html_encode_js_message(buf, sizeof(buf), msg_size);
  if (n < 0)
    return n;

  if (msgstream_fd_send(fd, buf, sizeof(buf), n))
    return -1;

  if (write(fd, msg, msg_size) == -1)
    return -1;

  return msg_size;
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

static int html_decode_upload_msg(cJSON *obj, struct begin_upload *msg) {
  // url: string
  // size: number
  // archive: bool
  if (!copy_string(obj, "url", msg->url, sizeof(msg->url)))
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

  msg->is_archive = 0;
  cJSON *archive = cJSON_GetObjectItem(obj, "archive");
  if (cJSON_IsTrue(archive))
    msg->is_archive = 1;

  return 1;
}

static int html_decode_navigate_msg(cJSON *obj, struct navigate *msg) {
  if (!copy_string(obj, "url", msg->url, sizeof(msg->url)))
    return 0;

  return 1;
}

static int html_decode_js_msg(cJSON *obj, struct js_message *msg) {
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

static int html_decode_mime_msg(cJSON *obj, html_mime_map mimes) {
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

  if (type_val == HTML_BEGIN_UPLOAD) {
    msg->type = HTML_BEGIN_UPLOAD;
    ret = html_decode_upload_msg(obj, &msg->msg.upload);
  } else if (type_val == HTML_NAVIGATE) {
    msg->type = HTML_NAVIGATE;
    ret = html_decode_navigate_msg(obj, &msg->msg.navigate);
  } else if (type_val == HTML_JS_MESSAGE) {
    msg->type = HTML_JS_MESSAGE;
    ret = html_decode_js_msg(obj, &msg->msg.js_msg);
  } else if (type_val == HTML_MIME_MAP) {
    msg->type = HTML_MIME_MAP;
    msg->msg.mime = html_mime_map_alloc();
    ret = html_decode_mime_msg(obj, msg->msg.mime);
  } else {
    goto fail;
  }

  cJSON_Delete(obj);
  return ret;

fail:
  cJSON_Delete(obj);
  return 0;
}

int HTML_API html_encode_submit_form(void *data, size_t size,
                                     size_t content_length,
                                     const char *mime_type) {
  // mime: string
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_SUBMIT_FORM))
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

static int html_decode_submit_form_msg(cJSON *obj,
                                       struct html_begin_submit_form *msg) {
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

int html_encode_recv_js_msg(void *data, size_t size, size_t content_length) {
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_RECV_JS_MSG))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_encode_close_request(void *data, size_t size) {
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

static int html_decode_recv_js_msg(cJSON *obj,
                                   struct html_begin_recv_js_msg *msg) {
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

  if (type_val == HTML_SUBMIT_FORM) {
    msg->type = HTML_SUBMIT_FORM;
    ret = html_decode_submit_form_msg(obj, &msg->msg.form);
  } else if (type_val == HTML_RECV_JS_MSG) {
    msg->type = HTML_RECV_JS_MSG;
    ret = html_decode_recv_js_msg(obj, &msg->msg.js_msg);
  } else if (type_val == HTML_IMSG_CLOSE_REQ) {
    msg->type = HTML_IMSG_CLOSE_REQ;
  } else {
    goto fail;
  }

  cJSON_Delete(obj);
  return ret;

fail:
  cJSON_Delete(obj);
  return 0;
}

static int parse_dot_file(const char *target, size_t n, int offset,
                          int *dot_len, int *only_dots) {
  *dot_len = 0;
  *only_dots = 1;
  int i = offset;
  for (; i < n && target[i] != '/'; ++i) {
    if (target[i] == '.') {
      *dot_len += 1;
    } else {
      *only_dots = 0;
    }
  }

  return i;
}

static int rfind(const char *str, int offset, char search) {
  for (int i = offset; i >= 0; --i) {
    if (str[i] == search)
      return i;
  }

  return -1;
}

int html_parse_target(const char *target, char *session_id,
                      size_t session_id_len, char *normalized_path,
                      size_t norm_path_len) {
  int n = strlen(target);

  if (session_id_len < 1 || norm_path_len < 1)
    return 0; // not useful at all

  // find session id (first non-empty piece of target)
  int start = 0, i = 0, session_id_n = 0;
  for (; i < n; ++i) {
    if (target[i] == '/') {
      if (session_id_n)
        break; // done
    } else {
      if (session_id_n >= session_id_len - 1) {
        return 0; // can't fit session id
      } else {
        session_id[session_id_n++] = target[i];
      }
    }
  }

  // session id not found
  if (session_id_n < 1)
    return 0;

  session_id[session_id_n] = '\0';

  // normalize path ...
  int norm_path_n = 0;
  while (i < n) {
    switch (target[i]) {
    case '@':
    case '%':
    case '+':
      return 0;
    default:
      break;
    }

    // virtual abs path
    if (target[i] == '~') {
      if (target[i - 1] == '/')
        norm_path_n = 1;
      else
        return 0;

      if ((i + 1) < n && target[i + 1] != '/')
        return 0;

      ++i;
      continue;
    }

    // deal with '.+' directories
    if (target[i] == '.' && target[i - 1] == '/') {
      int dot_len, only_dots;
      i = parse_dot_file(target, n, i, &dot_len, &only_dots);

      if (!only_dots)
        return 0; // hidden files not found

      if (dot_len == 1) {
        continue;
      } else if (dot_len == 2) {
        int current_dir_i = rfind(normalized_path, norm_path_n - 1, '/');
        int parent_dir_i = rfind(normalized_path, current_dir_i - 1, '/');
        if (parent_dir_i >= 0)
          norm_path_n = parent_dir_i + 1;

        continue;
      } else {
        return 0; // not handled
      }
    }

    if (norm_path_n >= norm_path_len - 1)
      return 0;

    // ignore multiple '/'
    if (!(target[i] == '/' && norm_path_n > 0 &&
          normalized_path[norm_path_n - 1] == '/')) {
      normalized_path[norm_path_n++] = target[i];
    }

    ++i;
  }

  if (norm_path_n < 1) {
    normalized_path[0] = '/';
    norm_path_n = 1;
  }

  if (normalized_path[norm_path_n - 1] == '/') {
    size_t remaining = norm_path_len - norm_path_n;
    size_t index_n =
        strlcpy(normalized_path + norm_path_n, "index.html", remaining);
    if (index_n > remaining)
      return 0; // couldn't fit

    norm_path_n += index_n;
  }

  normalized_path[norm_path_n] = '\0';
  return 1;
}

static int html_read_form_data(int fd, void *data, size_t size,
                               size_t *pnread) {
  uint8_t buf[HTML_MSG_SIZE];
  size_t n;
  if (msgstream_fd_recv(fd, buf, sizeof(buf), &n))
    return HTML_ERROR;

  struct html_in_msg msg;
  if (!html_decode_in_msg(buf, n, &msg))
    return HTML_ERROR;

  if (msg.type != HTML_SUBMIT_FORM) {
    return msg.type == HTML_IMSG_CLOSE_REQ ? HTML_CLOSE_REQ : HTML_ERROR;
  }

  struct html_begin_submit_form *form = &msg.msg.form;
  if (strcmp(form->mime_type, "application/x-www-form-urlencoded") != 0) {
    return HTML_ERROR;
  }

  if (form->content_length + 1 > size)
    return HTML_ERROR;

  int nread = 0;
  while (nread < form->content_length) {
    ssize_t ret = read(fd, data + nread, form->content_length - nread);
    if (ret < 1)
      return HTML_ERROR;

    nread += ret;
  }

  ((char *)data)[nread] = '\0';

  *pnread = nread;
  return HTML_OK;
}

// TODO - return val to bool
int HTML_API html_recv_js_message(html_connection *con, void *data,
                                  size_t size) {
  if (!con)
    return -1;

  int fd = con->fd;
  uint8_t buf[HTML_MSG_SIZE];
  size_t n;

  if (msgstream_fd_recv(fd, buf, sizeof(buf), &n))
    return -1;

  struct html_in_msg msg;
  if (!html_decode_in_msg(buf, n, &msg))
    return -1;

  if (msg.type != HTML_RECV_JS_MSG)
    return -1;

  struct html_begin_recv_js_msg *js_msg = &msg.msg.js_msg;
  if (js_msg->content_length + 1 > size) {
    return -1;
  }

  // TODO - factor out readn
  int nread = 0;
  while (nread < js_msg->content_length) {
    ssize_t ret = read(fd, data + nread, js_msg->content_length - nread);
    if (ret < 1)
      return -1;

    nread += ret;
  }

  ((char *)data)[nread] = '\0';
  return nread;
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

enum html_error_code html_read_form(html_connection *con, html_form *pform) {
  if (!(con && pform))
    return HTML_ERROR;

  char buf[HTML_FORM_SIZE];
  size_t n;
  int ec = html_read_form_data(con->fd, buf, sizeof(buf), &n);
  if (ec != HTML_OK)
    return ec;

  int nfields = n > 0 ? 1 : 0;
  for (int i = 0; i < n; ++i) {
    if (buf[i] == '&')
      ++nfields;
  }

  html_form form;
  if ((form = malloc(sizeof(struct html_form_))) == NULL)
    return HTML_ERROR;

  form->size = nfields;
  if ((form->fields = calloc(nfields, sizeof(struct html_form_field))) == NULL)
    return HTML_ERROR;

  int i = 0;
  for (int field_i = 0; field_i < nfields; ++field_i) {
    int field_size = parse_field(buf, n, i, &form->fields[field_i]);
    if (field_size < 0)
      return -1;

    i += field_size + 1; // magic 1 for '&'
  }

  *pform = form;
  return HTML_OK;
}

void html_form_release(html_form *pform) {
  if (!pform)
    return;

  html_form form = *pform;
  if (!form)
    return;

  for (size_t i = 0; i < form->size; ++i) {
    struct html_form_field *field = &form->fields[i];
    free(field->name);
    free(field->value);
  }

  free(form->fields);
  free(form);
  *pform = NULL;
}

size_t html_form_size(const html_form form) {
  if (!form)
    return 0;
  return form->size;
}

const char *HTML_API html_form_field_name(const html_form form, size_t i) {
  if (!(form && i < form->size))
    return NULL;
  return form->fields[i].name;
}

const char *HTML_API html_form_field_value(const html_form form, size_t i) {
  if (!(form && i < form->size))
    return NULL;
  return form->fields[i].value;
}

const char *HTML_API html_form_value_of(const html_form form,
                                        const char *field_name) {
  if (!form)
    return NULL;

  for (size_t i = 0; i < form->size; ++i) {
    if (strcmp(field_name, form->fields[i].name) == 0)
      return form->fields[i].value;
  }

  return NULL;
}

html_mime_map HTML_API html_mime_map_alloc() {
  html_mime_map ptr = malloc(sizeof(struct html_mime_map_));
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

void HTML_API html_mime_map_free(html_mime_map *pmimes) {
  if (!(pmimes && *pmimes))
    return;

  html_mime_map mimes = *pmimes;
  cJSON_Delete(mimes->array);
  free(mimes);
  *pmimes = NULL;
}

int html_mime_map_add(html_mime_map mimes, const char *extname,
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

int html_encode_upload_mime_map(void *data, size_t size, html_mime_map mimes) {
  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_MIME_MAP))
    return -1;

  if (!cJSON_AddItemReferenceToObject(obj, "map", mimes->array)) {
    return -1;
  }

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_upload_mime_map(html_connection *con, html_mime_map mimes) {
  if (!(con && mimes))
    return 0;

  char buf[HTML_MSG_SIZE];
  int n = html_encode_upload_mime_map(buf, sizeof(buf), mimes);
  if (n < 0)
    return 0;

  if (msgstream_fd_send(con->fd, buf, sizeof(buf), n))
    return 0;

  return 1;
}

size_t html_mime_map_size(html_mime_map mimes) {
  if (!mimes)
    return 0;
  return cJSON_GetArraySize(mimes->array);
}

int html_mime_map_entry_at(html_mime_map mimes, size_t i, const char **extname,
                           const char **mime_type) {
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
