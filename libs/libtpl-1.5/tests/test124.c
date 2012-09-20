#include <stdio.h>
#include <string.h>
#include "tpl.h"
#define LEN 10

const char *filename = "/tmp/test124.tpl";

typedef struct {
  char name[LEN];
} test_t;
int main() {
  test_t t;
  char *s;
  tpl_node *tn;

  tn = tpl_map("A(S(c#)s)", &t, LEN, &s);
  printf("mapped\n");

  memcpy(t.name,"abcdefghi\0",10);
  s="first";
  tpl_pack(tn,1);

  memcpy(t.name,"jklmnopqr\0",10);
  s="second";
  tpl_pack(tn,1);

  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);
  printf("freed\n");

  tn = tpl_map("A(S(c#)s)", &t, LEN, &s);
  tpl_load(tn,TPL_FILE,filename);
  while(tpl_unpack(tn,1) > 0) {
      printf("%s %s\n", t.name, s);
  }
  tpl_free(tn);
  return 0;
}
