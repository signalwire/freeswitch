#include "tpl.h"
#include <stdio.h>

const char *filename = "/tmp/test100.tpl";
int main() {
  tpl_node *tn;
  struct {
    int i;
    char c;
  } s;
  int p;
  char *fmt;

  tn = tpl_map("S(ic)", &s);
  s.i = 1; s.c = '^';
  tpl_pack(tn, 0);
  tpl_dump(tn, TPL_FILE, filename);
  tpl_free(tn);

  fmt = tpl_peek(TPL_FILE|TPL_DATAPEEK, filename, "i", &p);
  if (fmt) {
    printf("fmt: %s\n", fmt);
    printf("p: %d\n", p);
  }

  return 0;
}
