#include "tpl.h"
#include <stdio.h>

const char *filename="/tmp/test93.tpl";
int main() {
  tpl_node *tn;
  char *s = NULL;
  tn = tpl_map("s", &s);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  s = (char*)0x1; /* overwritten below */
  tn  = tpl_map("s", &s);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);
  printf("s %s null\n", (s==NULL?"is":"is NOT"));
  return(0);
}

