#include "html-forms.h"

#include <cjson/cJSON.h>

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

int html_decode_msg(const void *data, size_t size, struct html_msg *msg) {
  return 0;
}
