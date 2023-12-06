#include "html-forms.h"
#include "msgstream.h"

#include <catui.h>
#include <cjson/cJSON.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

/*
enum msg_type { HTML_BEGIN_UPLOAD = 0, HTML_PROMPT = 1 };

struct begin_upload {
  char url[HTML_URL_SIZE];
  size_t content_length;
  char mime_type[HTML_MIME_SIZE];
};

struct prompt {
  char url[HTML_URL_SIZE];
};

struct html_msg {
  enum msg_type type;
  union {
    struct begin_upload upload;
    struct prompt prompt;
  } msg;
};
*/
int html_connect(FILE *err) {
  return catui_connect("com.gulachek.html-forms", "0.1.0", err);
}

msgstream_size html_encode_upload(void *data, size_t size, const char *url,
                                  size_t content_length,
                                  const char *mime_type) {
  // url: string
  // mime: string
  // size: number

  cJSON *obj = cJSON_CreateObject();
  if (!obj)
    return MSGSTREAM_ERR;

  if (!cJSON_AddNumberToObject(obj, "type", HTML_BEGIN_UPLOAD))
    return MSGSTREAM_ERR;

  if (!cJSON_AddNumberToObject(obj, "size", content_length))
    return MSGSTREAM_ERR;

  if (!cJSON_AddStringToObject(obj, "mime", mime_type))
    return MSGSTREAM_ERR;

  if (!cJSON_AddStringToObject(obj, "url", url))
    return MSGSTREAM_ERR;

  if (!cJSON_PrintPreallocated(obj, data, size, 0))
    return MSGSTREAM_ERR;

  cJSON_Delete(obj);
  return strlen(data);
}

int html_upload(msgstream_fd fd, const char *url, const char *file_path,
                const char *mime_type) {
  struct stat stats;
  if (stat(file_path, &stats) == -1) {
    return -1;
  }

  char buf[HTML_MSG_SIZE];
  msgstream_size n =
      html_encode_upload(buf, sizeof(buf), url, stats.st_size, mime_type);
  if (n < 0)
    return n;

  if (msgstream_send(fd, buf, sizeof(buf), n, NULL) < 0)
    return -1;

  FILE *f = fopen(file_path, "r");
  if (!f)
    return -1;

  size_t nleft = stats.st_size;
  while (nleft) {
    size_t n_to_read = HTML_MSG_SIZE < nleft ? HTML_MSG_SIZE : nleft;
    size_t nread = fread(buf, 1, n_to_read, f);
    if (nread == 0)
      return -1;

    nleft -= nread;

    if (write(fd, buf, nread) == -1) {
      perror("write");
      return -1;
    }
  }

  return stats.st_size;
}

msgstream_size html_encode_prompt(void *data, size_t size, const char *url) {
  return -1;
}

int html_prompt(msgstream_fd fd, const char *url) {
  char buf[HTML_MSG_SIZE];
  msgstream_size n = html_encode_prompt(buf, sizeof(buf), url);
  if (n < 0)
    return n;

  return msgstream_send(fd, buf, sizeof(buf), n, NULL);
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

static int html_decode_prompt_msg(cJSON *obj, struct prompt *msg) {
  if (!copy_string(obj, "url", msg->url, sizeof(msg->url)))
    return 0;

  return 1;
}

int html_decode_msg(const void *data, size_t size, struct html_msg *msg) {
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
  } else if (type_val == HTML_PROMPT) {
    msg->type = HTML_PROMPT;
    ret = html_decode_prompt_msg(obj, &msg->msg.prompt);
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
