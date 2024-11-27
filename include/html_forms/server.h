#ifndef HTML_FORMS_SERVER_H
#define HTML_FORMS_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HTML_API
#define HTML_API
#endif

struct html_forms_server_;
typedef struct html_forms_server_ html_forms_server;

html_forms_server *HTML_API html_forms_server_init(unsigned short port);
void HTML_API html_forms_server_free(html_forms_server *server);

int HTML_API html_forms_server_run(html_forms_server *server);
int HTML_API html_forms_server_stop(html_forms_server *server);

int HTML_API html_forms_server_connect(html_forms_server *server, int fd);

#ifdef __cplusplus
}
#endif

#endif
