#ifndef HTML_FORMS_H
#define HTML_FORMS_H
/**
 * @file
 * @brief HTML Forms C/C++ Public API
 */

#include <stdio.h>
#include <stdlib.h>

#ifndef HTML_API
/** @cond PRIVATE Symbol is visible when included */
#define HTML_API
/** @endcond */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Size of message buffers for `com.gulachek.html-forms` protocol
 */
#define HTML_MSG_SIZE 2048

/**
 * Max size of URLs for communication
 */
#define HTML_URL_SIZE 512

/**
 * Max size of MIME types for communication
 */
#define HTML_MIME_SIZE 256

/**
 * Max size of HTML form submissions for communication
 * @remark This is referring to the POST body size
 */
#define HTML_FORM_SIZE 4096

/**
 * Error codes returned by the library.
 */
enum html_error_code {
  HTML_OK = 0,    /**< No error */
  HTML_ERROR = 1, /**< Error occurred */
  HTML_CLOSE_REQ =
      2, /**< A close was requested while an operation was in progress */
};

/** Input message types */
enum html_in_msg_type {
  HTML_SUBMIT_FORM = 0,    /**< Form submission */
  HTML_RECV_JS_MSG = 1,    /**< Application-defined message */
  HTML_IMSG_CLOSE_REQ = 2, /**< Request to close application */
};

/** Output message types */
enum html_out_msg_type {
  HTML_BEGIN_UPLOAD = 0, /**< Upload resources */
  HTML_NAVIGATE = 1,     /**< Navigate to a relative URL */
  HTML_JS_MESSAGE = 2,   /**< Application-defined message */
  HTML_MIME_MAP = 3      /**< Map file extensions to MIME types */
};

/** @cond PRIVATE Remove these typedefs */
typedef char html_mime_buf[HTML_MIME_SIZE];
typedef char html_url_buf[HTML_URL_SIZE];
typedef char html_form_buf[HTML_FORM_SIZE];
/** @endcond */

/** Upload resources */
struct begin_upload {
  size_t content_length; /**< The file or archive size in bytes */
  int is_archive;        /**< 1 if archive, 0 if regular file */
  html_url_buf url;      /**< URL to point at the resource */
};

/** Navigate to a relative URL */
struct navigate {
  html_url_buf url; /**< Relative URL */
};

/** Send an application-defined message */
struct js_message {
  size_t content_length; /**< @brief Size of message in bytes */
};

/** @cond PRIVATE */
struct html_mime_map_;
struct html_connection_;
struct html_form_;
/** @endcond */

/**
 * @typedef html_mime_map
 * Object to map file extensions to MIME types. See @ref mime_swap/main.c
 */
typedef struct html_mime_map_ html_mime_map;

/**
 * @typedef html_connection
 * Object that represents a connection to a `com.gulachek.html-forms` server
 * session. See @ref snake/main.cpp
 */
typedef struct html_connection_ html_connection;

/**
 * @typedef html_form
 * Object that holds a one-to-many mapping representing a submitted form. See
 * @ref todo/main.c
 */
typedef struct html_form_ html_form;

/**
 * Output message for use by server implementations
 */
struct html_out_msg {
  /** The message type */
  enum html_out_msg_type type;
  /** The message payload as a union */
  union {
    struct begin_upload upload; /**< @brief The upload payload */
    struct navigate navigate;   /**< @brief The navigate payload */
    struct js_message js_msg;   /**< @brief The message payload */

    /**
     * The message as a mime map
     * @remark The consumer is responsible for freeing the resources associated
     * with the object with @ref html_mime_map_free.
     */
    html_mime_map *mime;
  } msg;
};

/** User submitted a form */
struct html_begin_submit_form {
  /** The size of the form submission POST request body in bytes */
  size_t content_length;

  /**
   * The MIME type of the submitted form.
   * @remark The only currently supported type is
   * application/x-www-form-urlencoded
   */
  html_mime_buf mime_type;
};

/** User sent an application-defined message */
struct html_begin_recv_js_msg {
  /** The size of the message in bytes */
  size_t content_length;
};

/**
 * An input message from the user. Client libraries can use this to decode
 * messages and implement abstractions.
 */
struct html_in_msg {
  /** The message type */
  enum html_in_msg_type type;
  /** The message payload as a union */
  union {
    /** Submitted form */
    struct html_begin_submit_form form;
    /** Application-defined message */
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
 * @param[in] dst_size The size of the dst buffer, including space for the
 * null terminator
 * @param[in] src The null terminated string to escape
 * @return The size in bytes of the escaped string
 * @remark This is based on php's htmlspecialchars.
 * @remark If src is a null pointer, dst becomes an empty null terinated
 * string.
 */
size_t HTML_API html_escape(char *dst, size_t dst_size, const char *src);

/**
 * Connect to catui server with `com.gulachek.html-forms` protocol
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
 * @param[in] con The connection
 * @return A pointer to a null terminated string error message
 * @remark The pointer will be valid until another function operates on the
 * connection
 */
const char *HTML_API html_errmsg(html_connection *con);

/**
 * Encode an upload message
 * @param[in] data Pointer to buffer to hold encoded message
 * @param[in] size The size of @a data's buffer in bytes
 * @param[in] url The URL that the browser can request that will point to the
 * associated resource
 * @param[in] content_length The size of the uploaded file or archive in bytes
 * @param[in] is_archive 1 if the resource is an archive, 0 if a file
 * @return The size of the encoded message or -1 on failure
 */
int HTML_API html_encode_upload(void *data, size_t size, const char *url,
                                size_t content_length, int is_archive);

/**
 * Upload file to be accessible from URL
 * @param con The connection
 * @param url The url that the file will be accessible from the browser
 * @param file_path The file path on the application system to upload
 * content for
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
 * Uploads archive (like archive.tar.gz) to make contents accessible from
 * URL. See @ref tarball/main.c
 * @param con The connection
 * @param url The base URL of the archived directory
 * @param archive_path The path to the archive on the application system
 * @return 0 on failure, 1 on success
 */
int HTML_API html_upload_archive(html_connection *con, const char *url,
                                 const char *archive_path);

/**
 * Create a mime map object
 * @return A pointer to a newly created mime map, or a NULL pointer on
failure
 * @remark The caller must call html_mime_map_free() when done with the mime
map to avoid memory leaks.
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
 * @remark File extensions may include or omit the '.' character. For
 * example associating either ".html5" or "html5" with "text/html" will both
 * equally cause a request to
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

/**
 * Encode a navigation message
 * @param[in] data A buffer of size @a size to hold the encoded message
 * @param[in] size The size in bytes of the buffer pointed to by @a data
 * @param[in] url A null terminated string of the relative URL to navigate to
 * @return The size of the encoded message in bytes, -1 on failure
 */
int HTML_API html_encode_navigate(void *data, size_t size, const char *url);

/**
 * Navigate the user's session to a relative URL
 * @param[in] con The connection
 * @param[in] url A null terminated string of the relative URL to navigate to
 * @return 1 on success, 0 on failure
 */
int HTML_API html_navigate(html_connection *con, const char *url);

/**
 * Encode a header to send an application-defined message
 * @param[in] data Points to a buffer of size @a size bytes
 * @param[in] size The size of the buffer pointed to by @a data
 * @param[in] content_length The size in bytes of the application-defined
 * message to be sent
 * @return The size in bytes of the encoded message, -1 on failure
 */
int HTML_API html_encode_js_message(void *data, size_t size,
                                    size_t content_length);

/**
 * Send an application-defined message to the user
 * @param con The connection
 * @param msg A null terminated string of the message to send
 * @return A non-negative number on success, -1 on failure
 */
int HTML_API html_send_js_message(html_connection *con, const char *msg);

/**
 * Receive an application-defined message from the user
 * @param con The connection
 * @param data Pointer to a buffer of size @a size bytes
 * @param size Size in bytes of buffer pointed to by @a data
 * @return The size of the received message in bytes, -1 on failure
 */
int HTML_API html_recv_js_message(html_connection *con, void *data,
                                  size_t size);

/**
 * Decode an output message. This is useful for server implementations.
 * @param[in] data Pointer to a buffer with an encoded message
 * @param[in] size The size of the encoded message in bytes
 * @param[out] msg A structure that will store parsed information from the
 * encoded message
 * @return 1 on success, 0 on failure
 * @remark It's unlikely that you will use this.
 * @remark See @ref html_out_msg for specifics on messages and if resources need
 * to be released to avoid memory leaks.
 */
int HTML_API html_decode_out_msg(const void *data, size_t size,
                                 struct html_out_msg *msg);

/**
 * Decode an input message. This is useful for client library implementations.
 * @param[in] data Pointer to a buffer with an encoded message
 * @param[in] size The size of the encoded message in bytes
 * @param[out] msg A structure that will store parsed information from the
 * encoded message
 * @return 1 on success, 0 on failure
 * @remark It's unlikely that you should use this. You should look at more
 * specific APIs like @ref html_read_form
 * @remark See @ref html_in_msg for specifics on messages and if resources need
 * to be released to avoid memory leaks.
 */
int HTML_API html_decode_in_msg(const void *data, size_t size,
                                struct html_in_msg *msg);

/**
 * Encode a form submission. This is useful for server implementations.
 * @param[in] data Pointer to buffer to hold encoded message
 * @param[in] size The size in bytes of the buffer pointed to by @a data
 * @param[in] content_length The size of the form submission's POST request body
 * in bytes
 * @param[in] mime_type The null terminated string of the MIME type of the form
 * payload.
 * @remark Only `application/x-www-form-urlencoded` @a mime_type is expected for
 * clients to implement handling for
 * @return The size of the message in bytes, -1 on failure
 */
int HTML_API html_encode_submit_form(void *data, size_t size,
                                     size_t content_length,
                                     const char *mime_type);

/**
 * Encode an application-defined message header for the client to receive
 * @param[in] data Pointer to buffer to hold encoded message
 * @param[in] size Size in bytes of buffer pointed to by @a data
 * @param[in] content_length Size in bytes of the application-defined message
 */
int HTML_API html_encode_recv_js_msg(void *data, size_t size,
                                     size_t content_length);

/**
 * Encode a request to the client to close the application
 * @param[in] data Pointer to buffer to hold encoded message
 * @param[in] size Size in bytes of buffer pointed to by @a data
 * @return The size in bytes of the encoded message or -1 on failure
 */
int HTML_API html_encode_close_request(void *data, size_t size);

/**
 * Read an `application/x-www-form-urlencoded` form
 * @param[in] con The connection to read from
 * @param[out] pform Pointer to form pointer
 * @return HTML_OK when a form is read, other status otherwise
 * @remark The caller is responsible to free the form with html_form_free()
 */
enum html_error_code HTML_API html_read_form(html_connection *con,
                                             html_form **pform);

/**
 * Free a form object
 * @param[in] form The form to free
 */
void HTML_API html_form_free(html_form *form);

/**
 * Get the number of fields in a form
 * @param[in] form The form to query
 * @return The number of fields in the form
 */
size_t HTML_API html_form_size(const html_form *form);

/**
 * Get the name of a field
 * @param[in] form The form whose field is being queried
 * @param[in] i The index of the field being queried (starting at 0)
 * @return A pointer to a null-terminated string of the name of the field
 * @remark The returned pointer is valid until the form is freed
 */
const char *HTML_API html_form_name_at(const html_form *form, size_t i);

/**
 * Get the value of a field
 * @param[in] form The form whose field is being queried
 * @param[in] i The index of the field being queried (starting at 0)
 * @return A pointer to a null-terminated string of the value of the field
 * @remark The returned pointer is valid until the form is freed
 */
const char *HTML_API html_form_value_at(const html_form *form, size_t i);

/**
 * Get the value of a field with a specific name
 * @param[in] form The form to query
 * @param[in] field_name The name of the field to search for
 * @return A pointer to a null-terminated string of the value of the first
 * field whose name compares equal to field_name
 * @remark The returned pointer is valid until the form is freed
 */
const char *HTML_API html_form_value_of(const html_form *form,
                                        const char *field_name);

/** @cond PRIVATE */
int HTML_API html_parse_target(const char *target, char *session_id,
                               size_t session_id_len, char *normalized_path,
                               size_t norm_path_len);
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif
