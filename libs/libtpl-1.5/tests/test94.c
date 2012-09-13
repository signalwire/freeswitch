#include "tpl.h"
#include <stdio.h>

const char *filename="/tmp/test94.tpl";
int main() {
  tpl_node *tn;
  int i;
  char *s = NULL;
  tn = tpl_map("A(s)", &s);
  for(i=0;i<5;i++) {
    s = (i&1) ? NULL : "hello"; /* odd i are NULL string */
    tpl_pack(tn,1);
  }
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  s = (char*)0x1; /* overwritten below */
  tn  = tpl_map("A(s)", &s);
  tpl_load(tn,TPL_FILE,filename);
  while( tpl_unpack(tn,1) > 0) {
    printf("s is %s\n", (s?s:"NULL"));
  }
  tpl_free(tn);
  return(0);
}

