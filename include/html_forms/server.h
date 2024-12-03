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

int HTML_API html_forms_server_connect(html_forms_server *server, int fd);
int HTML_API html_forms_server_close_window(html_forms_server *server,
                                            int window_id);

#ifdef __cplusplus
}
#endif

#endif
