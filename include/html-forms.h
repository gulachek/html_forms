#ifndef HTML_FORMS_H
#define HTML_FORMS_H

#include <stdio.h>
#include <stdlib.h>

#ifndef HTML_API
#define HTML_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
typedef struct html_mime_map_ html_mime_map;

struct html_out_msg {
  enum html_out_msg_type type;
  union {
    struct begin_upload upload;
    struct navigate navigate;
    struct js_message js_msg;
    html_mime_map *mime;
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
typedef struct html_connection_ html_connection;

/**
 * Connect to catui server with 'com.gulachek.html-forms' protocol
 * @param[out] pcon Pointer to connection object pointer
 * @return 0 on failure, 1 on success
 */
int HTML_API html_connect(html_connection **pcon);

/**
 * Disconnect and free resources associated with a connection
 * @param[in] con The connection to be disconnected
 */
void HTML_API html_disconnect(html_connection *con);

/**
 * Retreive the last error message associated with the connection
 * @param[in] The connection
 * @return A pointer to a null terminated string error message
 * @remark The pointer will be valid until another function operates on the
 * connection
 */
const char *HTML_API html_errmsg(html_connection *con);

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
int HTML_API html_upload_file(html_connection *con, const char *url,
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
int HTML_API html_upload_dir(html_connection *con, const char *url,
                             const char *dir_path);

/**
 * Uploads archive (like archive.tar.gz) to make contents accessible from URL
 * @param con The connection
 * @param url The base URL of the archived directory
 * @param archive_path The path to the archive on the application system
 * @return 0 on failure, 1 on success
 */
int HTML_API html_upload_archive(html_connection *con, const char *url,
                                 const char *archive_path);

/**
 * Create a mime map object
 * @return A pointer to a newly created mime map, or a NULL pointer on failure
 * @remark The caller must call html_mime_map_free() when done with the mime map
to avoid memory leaks.
 */
html_mime_map *HTML_API html_mime_map_create();

/**
 * Free a mime map object
 * @param[in] mimes Pointer to mime map object
 */
void HTML_API html_mime_map_free(html_mime_map *mimes);

/**
 * Add a mapping from a URL file extension to a MIME type
 * @param[in] mimes The mime map object to be updated
 * @param[in] extname The file extension to map
 * @param[in] mime_type The corresponding mime type
 * @return 1 if successful, 0 on failure
 * @remark File extensions may include or omit the '.' character. For example
 * associating either ".html5" or "html5" with "text/html" will both equally
 * cause a request to
 * "/index.html5" to be served with a "text/html" Content-Type.
 */
int HTML_API html_mime_map_add(html_mime_map *mimes, const char *extname,
                               const char *mime_type);

/**
 * Get the number of mime map entries
 * @param[in] mimes The mime map object
 * @return The number of entries associated with the mime map
 */
size_t HTML_API html_mime_map_size(const html_mime_map *mimes);

/**
 * Get the contents of a mime map entry
 * @param[in] mimes The mime map object
 * @param[in] i The index of the entry to query (starts at 0)
 * @param[out] extname The associated file extension
 * @param[out] mime_type The associated mime type
 * @return 1 if successful, 0 on failure
 */
int HTML_API html_mime_map_entry_at(const html_mime_map *mimes, size_t i,
                                    const char **extname,
                                    const char **mime_type);

/**
 * Encode a message to send a mime map to the server
 * @param[in] data The buffer to hold the encoded message
 * @param[in] size The size of the buffer
 * @param[in] mimes The mime map object to encode
 */
int HTML_API html_encode_mime_map_apply(void *data, size_t size,
                                        const html_mime_map *mimes);

/**
 * Send a mime map to be applied to the session
 * @param[in] con The connection to the server
 * @param[in] mimes The mime map object to apply
 * @return 1 on success, 0 on failure
 */
int HTML_API html_mime_map_apply(html_connection *con,
                                 const html_mime_map *mimes);

int HTML_API html_encode_navigate(void *data, size_t size, const char *url);

int HTML_API html_navigate(html_connection *con, const char *url);

int HTML_API html_encode_js_message(void *data, size_t size,
                                    size_t content_length);

int HTML_API html_send_js_message(html_connection *con, const char *msg);

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
typedef struct html_form_ html_form;

/**
 * Read a application/x-www-form-urlencoded form
 */
enum html_error_code HTML_API html_read_form(html_connection *con,
                                             html_form **pform);

void HTML_API html_form_free(html_form *form);

size_t HTML_API html_form_size(const html_form *form);
const char *HTML_API html_form_name_at(const html_form *form, size_t i);
const char *HTML_API html_form_value_at(const html_form *form, size_t i);
const char *HTML_API html_form_value_of(const html_form *form,
                                        const char *field_name);

int HTML_API html_parse_target(const char *target, char *session_id,
                               size_t session_id_len, char *normalized_path,
                               size_t norm_path_len);

int HTML_API html_recv_js_message(html_connection *con, void *data,
                                  size_t size);

#ifdef __cplusplus
}
#endif

#endif
