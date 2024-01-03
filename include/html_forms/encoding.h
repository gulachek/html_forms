#ifndef HTML_FORMS_ENCODING_H
#define HTML_FORMS_ENCODING_H
/**
 * @file
 * @brief Tools for encoding/decoding protocol messages
 */

#include "html_forms.h"

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
 * Encode a message to send a mime map to the server
 * @param[in] data The buffer to hold the encoded message
 * @param[in] size The size of the buffer
 * @param[in] mimes The mime map object to encode
 */
int HTML_API html_encode_mime_map_apply(void *data, size_t size,
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

#ifdef __cplusplus
}
#endif

#endif
