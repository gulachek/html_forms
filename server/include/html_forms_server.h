/**
 * Copyright 2025 Nicholas Gulachek
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */
#ifndef HTML_FORMS_SERVER_H
#define HTML_FORMS_SERVER_H

#include "html_forms/encoding.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HTML_API
#define HTML_API
#endif

/** Hold a UUID and a null terminator */
#define HTML_FORMS_SERVER_SESSION_ID_SIZE 37

struct html_forms_server_;
typedef struct html_forms_server_ html_forms_server;

html_forms_server *HTML_API html_forms_server_init(unsigned short port,
                                                   const char *session_dir);
void HTML_API html_forms_server_free(html_forms_server *server);

typedef struct {
  int type;
  union {
    struct {
      char session_id[HTML_FORMS_SERVER_SESSION_ID_SIZE];
      char msg[HTML_MSG_SIZE];
    } show_err;

    struct {
      char session_id[HTML_FORMS_SERVER_SESSION_ID_SIZE];
      char url[HTML_URL_SIZE];
    } open_url;

    struct {
      char session_id[HTML_FORMS_SERVER_SESSION_ID_SIZE];
    } close_win;

    struct {
      char session_id[HTML_FORMS_SERVER_SESSION_ID_SIZE];
      char token[HTML_UUID_SIZE];
    } accept_io_transfer;
  } data;
} html_forms_server_event;

#define HTML_FORMS_SERVER_EVENT_SHOW_ERROR 1
#define HTML_FORMS_SERVER_EVENT_OPEN_URL 2
#define HTML_FORMS_SERVER_EVENT_CLOSE_WINDOW 3
#define HTML_FORMS_SERVER_EVENT_ACCEPT_IO_TRANSFER 4

typedef void html_forms_server_event_callback(const html_forms_server_event *ev,
                                              void *ctx);

int HTML_API html_forms_server_set_event_callback(
    html_forms_server *server, html_forms_server_event_callback *cb, void *ctx);

int HTML_API html_forms_server_run(html_forms_server *server);
int HTML_API html_forms_server_stop(html_forms_server *server);

/**
 * Begin a new session with a consumer-provided session ID on connected fd
 * @param[in] server The server object
 * @param[in] session_id The null terminated session ID in UUID format. Should
 * not be easily guessable
 * @param[in] fd The connected fd (stream) that governs the session
 * @return 1 on success, 0 on failure
 */
int HTML_API html_forms_server_start_session(html_forms_server *server,
                                             const char *session_id, int fd);

int HTML_API html_forms_server_close_window(html_forms_server *server,
                                            const char *session_id);

#ifdef __cplusplus
}
#endif

#endif
