#include <assert.h>
#include <html_forms.h>

int main() {
  size_t sz = html_escape_size("2>1");
  assert(sz == 7);
  return 0;
}
