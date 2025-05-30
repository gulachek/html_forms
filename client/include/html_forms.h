/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef HTML_FORMS_H
#define HTML_FORMS_H
/**
 * @file
 * @brief HTML Forms C/C++ Public API
 */

#include <stdlib.h>

#ifndef HTML_API
/** @cond PRIVATE Symbol is visible when included */
#define HTML_API
/** @endcond */
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
 * Initialize a connection object using a file descriptor previously
 * connected to a catui server with `com.gulachek.html-forms` protocol.
 * @param[out] pcon Pointer to connection object pointer
 * @param[in] fd The fd associated with the previously bootstrapped connection
 * @remark This should only be used to create new connection objects
 */
int HTML_API html_connection_transfer_fd(html_connection **pcon, int fd);

/**
 * Get a file descriptor associated with a connection.
 * @param[in] con The connection
 * @return The fd the connection is associated with, otherwise -1
 * @remark This is useful for select loops. Manipulating the underlying file
 * stream results in undefined behavior.
 */
int HTML_API html_connection_fd(html_connection *con);

/**
 * Check to see if a close was requested on the connection
 * @param[in] con The connection
 * @return 1 if a close is requested, 0 otherwise
 * @remark Once a close is requested, this function will return 1 until @ref
 * html_reject_close is called on the connection. To accept the close request,
 * call @ref html_disconnect.
 */
int HTML_API html_close_requested(const html_connection *con);

/**
 * Reject a requested close
 * @param[in] con The connection
 * @remark This function does not communicate with the server. It only cleans up
 * state to track that a close request was handled and rejected so that
 * subsequent requests can be handled appropriately. To accept a close request
 * instead, call @ref html_disconnect.
 */
void HTML_API html_reject_close(html_connection *con);

/**
 * Retreive the last error message associated with the connection
 * @param[in] con The connection
 * @return A pointer to a null terminated string error message
 * @remark The pointer will be valid until another function operates on the
 * connection
 */
const char *HTML_API html_errmsg(html_connection *con);

/**
 * Open stream to upload contents that will be available from URL
 * @param con The connection
 * @param url The url that the file will be accessible from the browser
 * @return 1 on success, 0 otherwise
 */
int HTML_API html_upload_stream_open(html_connection *con, const char *url);

/**
 * Write contents to an upload stream
 * @param con The connection
 * @param data Pointer to buffer that holds data to be written
 * @param size The number of bytes contained in @a data to be written
 * @return 1 on success, 0 otherwise
 * @remark The caller cannot assume that the data will be sent to the server
 * with this call. The implementation may buffer the contents for efficiency and
 * sync when @ref html_upload_stream_close is called.
 */
int HTML_API html_upload_stream_write(html_connection *con, const void *data,
                                      size_t size);

/**
 * Close an upload stream. Ensure all written contents are sent.
 * @param con The connection
 * @return 1 on success, 0 otherwise
 */
int HTML_API html_upload_stream_close(html_connection *con);

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
 * Send a mime map to be applied to the session
 * @param[in] con The connection to the server
 * @param[in] mimes The mime map object to apply
 * @return 1 on success, 0 on failure
 */
int HTML_API html_mime_map_apply(html_connection *con,
                                 const html_mime_map *mimes);

/**
 * Navigate the user's session to a relative URL
 * @param[in] con The connection
 * @param[in] url A null terminated string of the relative URL to navigate to
 * @return 1 on success, 0 on failure
 */
int HTML_API html_navigate(html_connection *con, const char *url);

/**
 * Send an application-defined message to the user
 * @param[in] con The connection
 * @param[in] data Pointer to buffer holding application-defined message
 * @param[in] size Size in bytes of the message to send
 * @return 1 on success, 0 on failure
 */
int HTML_API html_send(html_connection *con, const void *data, size_t size);

/**
 * Receive an application-defined message from the user
 * @param[in] con The connection
 * @param[in] data Pointer to a buffer of size @a size bytes
 * @param[in] size Size in bytes of buffer pointed to by @a data
 * @param[out] msg_size Size in bytes of message received
 * @return 1 if an app message was read, 0 otherwise
 */
int HTML_API html_recv(html_connection *con, void *data, size_t size,
                       size_t *msg_size);

/**
 * Read an `application/x-www-form-urlencoded` form
 * @param[in] con The connection to read from
 * @param[out] pform Pointer to form pointer
 * @return 1 if a form was read, 0 otherwise
 * @remark The caller is responsible to free the form with html_form_free()
 */
int HTML_API html_form_read(html_connection *con, html_form **pform);

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

#ifdef __cplusplus
}
#endif

#endif
