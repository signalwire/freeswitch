#include <stdio.h>
#include "tpl.h"
#include <inttypes.h>

const char *filename = "/tmp/test109.tpl";

typedef struct {
  char c;
  uint32_t i;
  uint16_t j;
  char d;
} spad;

int main() {
  tpl_node *tn;
  spad s = {'a', 1, 2, 'b'}, t = {'?', 0, 0, '!'};;

  printf("sizeof(s): %d\n", (int)sizeof(s));;
  tn = tpl_map("S(cijc)", &s);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("S(cijc)", &t);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  if (s.c==t.c && s.i==t.i && s.j==t.j && s.d==t.d) 
    printf("structures match\n");
  else
    printf("structures mismatch\n");

  return 0;
}
