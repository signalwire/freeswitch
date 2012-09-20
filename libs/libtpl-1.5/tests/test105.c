#include "tpl.h"
#include <stdio.h>

const char *filename = "/tmp/test105.tpl";

int main() {
  int i=1;
  char *s="hello",*w="world",c='$', *fmt;
  tpl_node *tn;

  tpl_jot(TPL_FILE, filename, "issc", &i, &s, &w, &c);

  i = 0; s = NULL; w = NULL; c = 0;

  /* unpack the normal way */
  tn = tpl_map("issc", &i, &s, &w, &c);
  tpl_load(tn, TPL_FILE, filename);
  tpl_unpack(tn,0);
  tpl_free(tn);
  printf("i: %d, s: %s, w: %s, c: %c\n", i, s, w, c);

  i = 0; s = NULL; w = NULL; c = 0;

  /* unpack the quick way */
  fmt = tpl_peek(TPL_FILE|TPL_DATAPEEK, filename, "issc", &i, &s, &w, &c);
  printf("i: %d, s: %s, w: %s, c: %c\n", i, s, w, c);
  
  return 0;
}
