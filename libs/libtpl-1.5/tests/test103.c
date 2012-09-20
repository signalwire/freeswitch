#include "tpl.h"
#include <stdio.h>

const char *filename = "/tmp/test103.tpl";
int main() {
  tpl_node *tn;
  struct {
    int i;
    char *s;
    char c;
    char *t;
    int j;
    unsigned u;
  } s;
  char *fmt, *ps, pc, *pt;
  int pi, pj;

  tn = tpl_map("S(iscsiu)", &s);
  s.i = 1; s.s = "hello"; s.c = '^'; s.t = "world"; s.j = 2; s.u = 3;
  tpl_pack(tn, 0);
  tpl_dump(tn, TPL_FILE, filename);
  tpl_free(tn);

  fmt = tpl_peek(TPL_FILE|TPL_DATAPEEK, filename, "iscsi",&pi,&ps,&pc,&pt,&pj);
  if (fmt) {
    printf("fmt: %s\n", fmt);
    printf("pi: %d, ps: %s, pc: %c, pt: %s, pi: %d\n", pi,ps,pc,pt,pj);
  } else {
    printf("peek failed\n");
  }

  return 0;
}
