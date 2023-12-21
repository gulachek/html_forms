#ifndef HTML_FORMS_H
#define HTML_FORMS_H

#define HTML_MSG_SIZE 2048
#define HTML_URL_SIZE 512
#define HTML_MIME_SIZE 256
#define HTML_FORM_SIZE 4096

enum html_error_code {
  /* no error */
  HTML_OK = 0,
  /* error */
  HTML_ERROR = 1,
  /* user or program requested close while operation in progress */
  HTML_CLOSE_REQ = 2,
};

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
  HTML_JS_MESSAGE = 2,
  HTML_MIME_MAP = 3
};

typedef char html_mime_buf[HTML_MIME_SIZE];
typedef char html_url_buf[HTML_URL_SIZE];
typedef char html_form_buf[HTML_FORM_SIZE];

struct begin_upload {
  size_t content_length;
  int is_archive;
  html_url_buf url;
};

struct navigate {
  html_url_buf url;
};

struct js_message {
  size_t content_length;
};

struct html_mime_map_;
typedef struct html_mime_map_ *html_mime_map;

struct html_out_msg {
  enum html_out_msg_type type;
  union {
    struct begin_upload upload;
    struct navigate navigate;
    struct js_message js_msg;
    html_mime_map mime;
  } msg;
};

enum html_in_msg_type {
  HTML_SUBMIT_FORM = 0,
  HTML_RECV_JS_MSG = 1,
  HTML_IMSG_CLOSE_REQ = 2,
};

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

/**
 * Determines the size needed to allocate an HTML escaped string, including a
 * null terminator
 * @param[in] src The null terminated string to escape
 * @return The size in bytes of the escaped string
 * @remark If src is a null pointer, then 1 is returned for a single null
 * terminator.
 */
size_t HTML_API html_escape_size(const char *src);

/**
 * Escape a string to be safe to use as text in HTML
 * @param[out] dst A pointer to a buffer that will hold the escaped null
 * terminated string
 * @param[in] dst_size The size of the dst buffer, including space for the null
 * terminator
 * @param[in] src The null terminated string to escape
 * @return The size in bytes of the escaped string
 * @remark This is based on php's htmlspecialchars.
 * @remark If src is a null pointer, dst becomes an empty null terinated string.
 */
size_t HTML_API html_escape(char *dst, size_t dst_size, const char *src);

struct html_connection_;
typedef struct html_connection_ *html_connection;

html_connection HTML_API html_connection_alloc();
void HTML_API html_connection_free(html_connection *con);

int HTML_API html_connect(html_connection con);

int HTML_API html_encode_upload(void *data, size_t size, const char *url,
                                size_t content_length, int is_archive);

/**
 * Upload file to be accessible from URL
 * @param con The connection
 * @param url The url that the file will be accessible from the browser
 * @param file_path The file path on the application system to upload content
 * for
 * @return 0 on failure, 1 on success
 */
int HTML_API html_upload_file(html_connection con, const char *url,
                              const char *file_path);

/**
 * Recursively upload files to be accessible from URL
 * @param con The connection
 * @param url The base URL of the directory
 * @param dir_path The path to the directory on the application system to
 * iterate and upload content from
 * @return 0 on failure, 1 on success
 * @remark Hidden files that begin with '.' (like '.gitignore') will not be
 * uploaded
 */
int HTML_API html_upload_dir(html_connection con, const char *url,
                             const char *dir_path);

/**
 * Uploads archive (like archive.tar.gz) to make contents accessible from URL
 * @param con The connection
 * @param url The base URL of the archived directory
 * @param archive_path The path to the archive on the application system
 * @return 0 on failure, 1 on success
 */
int HTML_API html_upload_archive(html_connection con, const char *url,
                                 const char *archive_path);

html_mime_map HTML_API html_mime_map_alloc();
void HTML_API html_mime_map_free(html_mime_map *mimes);

int HTML_API html_mime_map_add(html_mime_map mimes, const char *extname,
                               const char *mime_type);

size_t HTML_API html_mime_map_size(html_mime_map mimes);
int HTML_API html_mime_map_entry_at(html_mime_map mimes, size_t i,
                                    const char **extname,
                                    const char **mime_type);

int HTML_API html_encode_upload_mime_map(void *data, size_t size,
                                         html_mime_map mimes);

int HTML_API html_upload_mime_map(html_connection con, html_mime_map mimes);

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

int HTML_API html_encode_close_request(void *data, size_t size);

struct html_form_;
typedef struct html_form_ *html_form;

/**
 * Read a application/x-www-form-urlencoded form
 */
enum html_error_code HTML_API html_read_form(html_connection con,
                                             html_form *form_ptr);

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
