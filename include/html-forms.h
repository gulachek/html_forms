#ifndef HTML_FORMS_H
#define HTML_FORMS_H

#define HTML_MSG_SIZE 2048
#define HTML_URL_SIZE 512
#define HTML_MIME_SIZE 128
#define HTML_PROMPT_SIZE 4096

#ifndef HTML_API
#define HTML_API
#endif

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

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

int HTML_API html_encode_upload(const char *url, size_t content_length,
                                const char *mime_type);

int HTML_API html_encode_prompt(const char *url);

int HTML_API html_decode_msg(const void *data, size_t size,
                             struct html_msg *msg);

#ifdef __cplusplus
}
#endif

#endif
