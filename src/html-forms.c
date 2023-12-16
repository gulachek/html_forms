#include "html-forms.h"
#include "html_connection.h"
#include <msgstream.h>

#include <catui.h>
#include <cjson/cJSON.h>
#include <ctype.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

html_connection html_connection_alloc() {
  html_connection con = malloc(sizeof(struct html_connection_));
  if (!con)
    return NULL;

  con->fd = -1;
  return con;
}

void html_connection_free(html_connection *con) {
  if (!(con && *con))
    return;

  free(*con);
  *con = NULL;
}

int html_connect(html_connection con) {
  if (!con)
    return 0;

  con->fd = catui_connect("com.gulachek.html-forms", "0.1.0", stderr);
  if (con->fd == -1) {
    // TODO - add error to connection
    return 0;
  }

  return 1;
}

int html_encode_upload(void *data, size_t size, const char *url,
                       size_t content_length, const char *mime_type) {
  // url: string
  // mime: string
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return -1;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_BEGIN_UPLOAD))
    return -1;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return -1;

  if (!cJSON_AddStringToObject(obj, "mime", mime_type))
    return -1;

  if (!cJSON_AddStringToObject(obj, "url", url))
    return -1;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return -1;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_upload(html_connection con, const char *url, const char *file_path,
                const char *mime_type) {
  if (!con)
    return 0;

  int fd = con->fd;

  struct stat stats;
  if (stat(file_path, &stats) == -1) {
    return 0;
  }

  char buf[HTML_MSG_SIZE];
  int n = html_encode_upload(buf, sizeof(buf), url, stats.st_size, mime_type);
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

int html_navigate(html_connection con, const char *url) {
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
int html_send_js_message(html_connection con, const char *msg) {
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
  // mime: string
  // size: number
  if (!copy_string(obj, "url", msg->url, sizeof(msg->url)))
    return 0;

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

int HTML_API html_encode_recv_js_msg(void *data, size_t size,
                                     size_t content_length) {
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

static int html_read_form_data(int fd, void *data, size_t size) {
  uint8_t buf[HTML_MSG_SIZE];
  size_t n;
  if (msgstream_fd_recv(fd, buf, sizeof(buf), &n))
    return -1;

  struct html_in_msg msg;
  if (!html_decode_in_msg(buf, n, &msg))
    return -1;

  if (msg.type != HTML_SUBMIT_FORM)
    return -1;

  struct html_begin_submit_form *form = &msg.msg.form;
  if (strcmp(form->mime_type, "application/x-www-form-urlencoded") != 0) {
    return -1;
  }

  if (form->content_length + 1 > size)
    return -1;

  int nread = 0;
  while (nread < form->content_length) {
    ssize_t ret = read(fd, data + nread, form->content_length - nread);
    if (ret < 1)
      return -1;

    nread += ret;
  }

  ((char *)data)[nread] = '\0';
  return nread;
}

// TODO - return val to bool
int HTML_API html_recv_js_message(html_connection con, void *data,
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

int html_read_form(html_connection con, html_form *pform) {
  if (!con)
    return -1;

  if (!pform)
    return -1;

  if (*pform)
    html_form_release(pform);

  char buf[HTML_FORM_SIZE];
  int n = html_read_form_data(con->fd, buf, sizeof(buf));
  if (n < 1)
    return n;

  int nfields = 1;
  for (int i = 0; i < n; ++i) {
    if (buf[i] == '&')
      ++nfields;
  }

  html_form form;
  if ((form = malloc(sizeof(struct html_form_))) == NULL)
    return -1;

  form->size = nfields;
  if ((form->fields = calloc(nfields, sizeof(struct html_form_field))) == NULL)
    return -1;

  int i = 0;
  for (int field_i = 0; field_i < nfields; ++field_i) {
    int field_size = parse_field(buf, n, i, &form->fields[field_i]);
    if (field_size < 0)
      return -1;

    i += field_size + 1; // magic 1 for '&'
  }

  *pform = form;
  return nfields;
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
