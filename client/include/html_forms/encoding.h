/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
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
 * Size of a null-terminated serialized UUID for communication
 */
#define HTML_UUID_SIZE 37

/**
 * Max size of HTML form submissions for communication
 * @remark This is referring to the POST body size
 */
#define HTML_FORM_SIZE 4096

/** Input message types */
enum html_in_msg_type {
  HTML_IMSG_FORM = 0,      /**< Form submission */
  HTML_IMSG_APP_MSG = 1,   /**< Application-defined message */
  HTML_IMSG_CLOSE_REQ = 2, /**< Request to close application */
  HTML_IMSG_ERROR = 3,     /**< Server reported error */
};

/** Output message types */
enum html_out_msg_type {
  HTML_OMSG_UPLOAD = 0,             /**< Upload resources */
  HTML_OMSG_NAVIGATE = 1,           /**< Navigate to a relative URL */
  HTML_OMSG_APP_MSG = 2,            /**< Application-defined message */
  HTML_OMSG_MIME_MAP = 3,           /**< Map file extensions to MIME types */
  HTML_OMSG_CLOSE = 4,              /**< Close the connection */
  HTML_OMSG_ACCEPT_IO_TRANSFER = 5, /**< Accept an I/O transfer request */
};

/** Resource types to be uploaded */
enum html_resource_type {
  HTML_RT_FILE = 0,    /**< Plain file is uploaded */
  HTML_RT_ARCHIVE = 1, /**< Archive (like .tar.gz) is uploaded */
};

/** Upload resources */
struct html_omsg_upload {
  size_t content_length;         /**< The file or archive size in bytes. A
                                    content_length of indicates that the upload will be
                                    transmitted in sized chunks. */
  enum html_resource_type rtype; /**< The resource type */
  char url[HTML_URL_SIZE];       /**< URL to point at the resource */
};

/** Navigate to a relative URL */
struct html_omsg_navigate {
  char url[HTML_URL_SIZE]; /**< Relative URL */
};

/** Send an application-defined message */
struct html_omsg_app_msg {
  size_t content_length; /**< @brief Size of message in bytes */
};

/** Accept an I/O transfer request */
struct html_omsg_accept_io_transfer {
  char token[HTML_UUID_SIZE]; /**< Token associated with I/O transfer request */
};

/**
 * Output message for use by server implementations
 */
struct html_out_msg {
  /** The message type */
  enum html_out_msg_type type;
  /** The message payload as a union */
  union {
    struct html_omsg_upload upload;     /**< @brief The upload payload */
    struct html_omsg_navigate navigate; /**< @brief The navigate payload */
    struct html_omsg_app_msg
        app_msg; /**< @brief The application-defined message payload */
    struct html_omsg_accept_io_transfer accept_io_transfer; /**< @brief The
                                            accept I/O transfer payload */

    /**
     * The message as a mime map
     * @remark The consumer is responsible for freeing the resources associated
     * with the object with @ref html_mime_map_free.
     */
    html_mime_map *mime;
  } msg;
};

/** User submitted a form */
struct html_imsg_form {
  /** The size of the form submission POST request body in bytes */
  size_t content_length;

  /**
   * The MIME type of the submitted form.
   * @remark The only currently supported type is
   * application/x-www-form-urlencoded
   */
  char mime_type[HTML_MIME_SIZE];
};

/** User sent an application-defined message */
struct html_imsg_app_msg {
  /** The size of the message in bytes */
  size_t content_length;
};

/** Server reported fatal error */
struct html_imsg_error {
  /** null-terminated message sent from server */
  char msg[HTML_MSG_SIZE];
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
    struct html_imsg_form form;
    /** Application-defined message */
    struct html_imsg_app_msg app_msg;
    /** Fatal error reported by server */
    struct html_imsg_error error;
  } msg;
};

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
 * specific APIs like @ref html_form_read
 * @remark See @ref html_in_msg for specifics on messages and if resources need
 * to be released to avoid memory leaks.
 */
int HTML_API html_decode_in_msg(const void *data, size_t size,
                                struct html_in_msg *msg);

/**
 * Encode an upload message
 * @param[in] data Pointer to buffer to hold encoded message
 * @param[in] size The size of @a data's buffer in bytes
 * @param[in] url The URL that the browser can request that will point to the
 * associated resource
 * @param[in] content_length The size of the uploaded file or archive in bytes.
 * 0 indicates that the upload will be streamed in sized chunks.
 * @param[in] type The type of resource being uploaded
 * @return The size of the encoded message or -1 on failure
 */
int HTML_API html_encode_omsg_upload(void *data, size_t size, const char *url,
                                     size_t content_length,
                                     enum html_resource_type type);

/**
 * Encode a message to send a mime map to the server
 * @param[in] data The buffer to hold the encoded message
 * @param[in] size The size of the buffer
 * @param[in] mimes The mime map object to encode
 */
int HTML_API html_encode_omsg_mime_map(void *data, size_t size,
                                       const html_mime_map *mimes);

/**
 * Encode a navigation message
 * @param[in] data A buffer of size @a size to hold the encoded message
 * @param[in] size The size in bytes of the buffer pointed to by @a data
 * @param[in] url A null terminated string of the relative URL to navigate to
 * @return The size of the encoded message in bytes, -1 on failure
 */
int HTML_API html_encode_omsg_navigate(void *data, size_t size,
                                       const char *url);

/**
 * Encode a header to send an application-defined message
 * @param[in] data Points to a buffer of size @a size bytes
 * @param[in] size The size of the buffer pointed to by @a data
 * @param[in] content_length The size in bytes of the application-defined
 * message to be sent
 * @return The size in bytes of the encoded message, -1 on failure
 */
int HTML_API html_encode_omsg_app_msg(void *data, size_t size,
                                      size_t content_length);

/**
 * Encode a close message
 * @param[in] data Points to a buffer of size @a size bytes
 * @param[in] size The size of the buffer pointed to by @a data
 * message to be sent
 * @return The size in bytes of the encoded message, -1 on failure
 */
int HTML_API html_encode_omsg_close(void *data, size_t size);

/**
 * Encode an accept I/O transfer message
 * @param[in] data Points to a buffer of size @a size bytes
 * @param[in] size The size of the buffer pointed to by @a data
 * @param[in] token The I/O transfer token as a null terminated UUID string
 * message to be sent
 * @return The size in bytes of the encoded message, -1 on failure
 */
int HTML_API html_encode_omsg_accept_io_transfer(void *data, size_t size,
                                                 const char *token);

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
int HTML_API html_encode_imsg_form(void *data, size_t size,
                                   size_t content_length,
                                   const char *mime_type);

/**
 * Encode an application-defined message header for the client to receive
 * @param[in] data Pointer to buffer to hold encoded message
 * @param[in] size Size in bytes of buffer pointed to by @a data
 * @param[in] content_length Size in bytes of the application-defined message
 */
int HTML_API html_encode_imsg_app_msg(void *data, size_t size,
                                      size_t content_length);

/**
 * Encode a request to the client to close the application
 * @param[in] data Pointer to buffer to hold encoded message
 * @param[in] size Size in bytes of buffer pointed to by @a data
 * @return The size in bytes of the encoded message or -1 on failure
 */
int HTML_API html_encode_imsg_close_req(void *data, size_t size);

/**
 * Encode a server-sent fatal error message
 * @param[in] str Pointer to buffer to hold encoded message
 * @param[in] size Size in bytes of buffer pointed to by @a data
 * @param[in] msg Null terminated string holding error message
 * @return The size in bytes of the encoded message or -1 on failure
 */
int HTML_API html_encode_imsg_error(void *data, size_t size, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
