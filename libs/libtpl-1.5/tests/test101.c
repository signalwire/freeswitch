#include "tpl.h"
#include <stdio.h>

extern tpl_hook_t tpl_hook;
const char *filename = "/tmp/test101.tpl";
int main() {
  tpl_node *tn;
  struct {
    int i;
    char c;
  } s;
  char *fmt, q;
  tpl_hook.oops = printf;

  tn = tpl_map("S(ic)", &s);
  s.i = 1; s.c = '^';
  tpl_pack(tn, 0);
  tpl_dump(tn, TPL_FILE, filename);
  tpl_free(tn);

  fmt = tpl_peek(TPL_FILE|TPL_DATAPEEK, filename, "c", &q);
  if (fmt) {
    printf("fmt: %s\n", fmt);
    printf("q: %c\n", q);
  } else {
    printf("peek failed\n");
  }

  return 0;
}
