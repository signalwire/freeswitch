#include "tpl.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  int i;
  char c[4];
} test_t;

const char *filename = "/tmp/test107.tpl";

int main() {
  test_t s[5], t[5];
  tpl_node *tn;
  int i;

  s[0].i = 0; strcpy(s[0].c, "cat");
  s[1].i = 1; strcpy(s[1].c, "dog");
  s[2].i = 2; strcpy(s[2].c, "eel");
  s[3].i = 3; strcpy(s[3].c, "emu");
  s[4].i = 4; strcpy(s[4].c, "ant");

  tn = tpl_map("S(ic#)#", &s, 4, 5);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("S(ic#)#", &t, 4, 5);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  for(i=0; i < 5; i++) {
    printf("%d %s\n", s[i].i, s[i].c);
  }

  return 0;
}
