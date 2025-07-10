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

struct html_forms_server_;
typedef struct html_forms_server_ html_forms_server;

html_forms_server *HTML_API html_forms_server_init(unsigned short port,
                                                   const char *session_dir);
void HTML_API html_forms_server_free(html_forms_server *server);

typedef struct {
  int type;
  union {
    struct {
      int window_id;
      char msg[HTML_MSG_SIZE];
    } show_err;

    struct {
      int window_id;
      char url[HTML_URL_SIZE];
    } open_url;

    struct {
      int window_id;
    } close_win;
  } data;
} html_forms_server_event;

#define HTML_FORMS_SERVER_EVENT_SHOW_ERROR 1
#define HTML_FORMS_SERVER_EVENT_OPEN_URL 2
#define HTML_FORMS_SERVER_EVENT_CLOSE_WINDOW 3

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

/**
 * Query the session ID associated with a browser window
 * @param[in] server The server object
 * @param[in] window_id The window whose session is being looked up
 * @param[out] session_id_buf A buffer pointing to memory to hold the null
 * terminated session ID
 * @param[in] The size of session_id_buf in chars. This should be at least 37 to
 * hold a UUID
 * @return 1 on success, 0 on failure
 */
int HTML_API html_forms_server_session_for_window(html_forms_server *server,
                                                  int window_id,
                                                  char *session_id_buf,
                                                  size_t session_id_bufsize);

int HTML_API html_forms_server_close_window(html_forms_server *server,
                                            int window_id);

#ifdef __cplusplus
}
#endif

#endif
