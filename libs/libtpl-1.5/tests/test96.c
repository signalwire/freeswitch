#include "tpl.h"
#include <stdio.h>

const char *filename="/tmp/test96.tpl";
int main() {
  tpl_node *tn;
  int i;
  char *s1 = NULL, *s2 = "", *s3 = "hello";
  tn = tpl_map("A(sss)", &s1, &s2, &s3);
  for(i=0;i<5;i++) {
    tpl_pack(tn,1);
  }
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  s1 = s2 = s3 = (char*)0x1; /* overwritten below */
  tn  = tpl_map("A(sss)", &s1, &s2, &s3);
  tpl_load(tn,TPL_FILE,filename);
  while( tpl_unpack(tn,1) > 0) {
    printf("s1 %s\n", s1?s1:"NULL");
    printf("s2 %s\n", s2?s2:"NULL");
    printf("s3 %s\n", s3?s3:"NULL");
  }
  tpl_free(tn);
  return(0);
}

