#ifndef HTML_FORMS_H
#define HTML_FORMS_H

#define HTML_MSG_SIZE 2048
#define HTML_URL_SIZE 512
#define HTML_MIME_SIZE 128
#define HTML_FORM_SIZE 4096

#ifndef HTML_API
#define HTML_API
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum html_out_msg_type {
  HTML_BEGIN_UPLOAD = 0,
  HTML_NAVIGATE = 1,
  HTML_JS_MESSAGE = 2
};

typedef char html_mime_buf[HTML_MIME_SIZE];
typedef char html_url_buf[HTML_URL_SIZE];
typedef char html_form_buf[HTML_FORM_SIZE];

struct begin_upload {
  size_t content_length;
  html_url_buf url;
  html_mime_buf mime_type;
};

struct navigate {
  html_url_buf url;
};

struct js_message {
  size_t content_length;
};

struct html_out_msg {
  enum html_out_msg_type type;
  union {
    struct begin_upload upload;
    struct navigate navigate;
    struct js_message js_msg;
  } msg;
};

enum html_in_msg_type { HTML_SUBMIT_FORM = 0, HTML_RECV_JS_MSG = 1 };

struct html_begin_submit_form {
  size_t content_length;
  html_mime_buf mime_type;
};

struct html_begin_recv_js_msg {
  size_t content_length;
};

struct html_in_msg {
  enum html_in_msg_type type;
  union {
    struct html_begin_submit_form form;
    struct html_begin_recv_js_msg js_msg;
  } msg;
};

struct html_connection_;
typedef struct html_connection_ *html_connection;

html_connection HTML_API html_connection_alloc();
void HTML_API html_connection_free(html_connection *con);

int HTML_API html_connect(html_connection con);

int HTML_API html_encode_upload(void *data, size_t size, const char *url,
                                size_t content_length, const char *mime_type);

int HTML_API html_upload(html_connection con, const char *url,
                         const char *file_path, const char *mime_type);

int HTML_API html_encode_navigate(void *data, size_t size, const char *url);

int HTML_API html_navigate(html_connection con, const char *url);

int HTML_API html_encode_js_message(void *data, size_t size,
                                    size_t content_length);

int HTML_API html_send_js_message(html_connection con, const char *msg);

int HTML_API html_decode_out_msg(const void *data, size_t size,
                                 struct html_out_msg *msg);

int HTML_API html_decode_in_msg(const void *data, size_t size,
                                struct html_in_msg *msg);

int HTML_API html_encode_submit_form(void *data, size_t size,
                                     size_t content_length,
                                     const char *mime_type);

int HTML_API html_encode_recv_js_msg(void *data, size_t size,
                                     size_t content_length);

struct html_form_;
typedef struct html_form_ *html_form;

/**
 * Read a application/x-www-form-urlencoded form
 */
int HTML_API html_read_form(html_connection con, html_form *form_ptr);
void HTML_API html_form_release(html_form *form_ptr);

size_t HTML_API html_form_size(const html_form form);
const char *HTML_API html_form_field_name(const html_form form, size_t i);
const char *HTML_API html_form_field_value(const html_form form, size_t i);
const char *HTML_API html_form_value_of(const html_form form,
                                        const char *field_name);

int HTML_API html_parse_target(const char *target, char *session_id,
                               size_t session_id_len, char *normalized_path,
                               size_t norm_path_len);

int HTML_API html_recv_js_message(html_connection con, void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
