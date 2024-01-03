#ifndef HTML_CONNECTION_H
#define HTML_CONNECTION_H

#include "html_forms.h"

struct html_connection_ {
  int fd;
  char errbuf[512];
};

int printf_err(html_connection *con, const char *fmt, ...);

#endif
