#include "tpl.h"
#include <stdio.h>

const char *filename = "/tmp/test102.tpl";
int main() {
  tpl_node *tn;
  char *fmt,p,q;

  tn = tpl_map("c", &p);
  p = '!';
  tpl_pack(tn, 0);
  tpl_dump(tn, TPL_FILE, filename);
  tpl_free(tn);

  fmt = tpl_peek(TPL_FILE|TPL_DATAPEEK, filename, "c", &q);
  if (fmt) {
    printf("fmt: %s\n", fmt);
    printf("q: %c\n", q);
  }

  return 0;
}
