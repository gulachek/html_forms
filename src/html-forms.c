#include "html-forms.h"

#include <cjson/cJSON.h>
#include <string.h>

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

int html_encode_upload(const char *url, size_t content_length,
                       const char *mime_type) {
  return 0;
}

int html_encode_prompt(const char *url) { return 0; }

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
    // deal with '.+' directories
    if (target[i] == '.' && target[i - 1] == '/') {
      int dot_len, only_dots;
      i = parse_dot_file(target, n, i, &dot_len, &only_dots);

      if (!only_dots)
        return 0; // hidden files not found

      if (dot_len == 1) {
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
