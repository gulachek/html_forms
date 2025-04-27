#include <assert.h>
#include <html_forms_server.h>

int main() {
  int ret = html_forms_server_stop(NULL);
  assert(ret == 0);
  return 0;
}
