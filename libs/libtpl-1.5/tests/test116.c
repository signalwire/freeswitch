#include <stdio.h>
#include "tpl.h"

#define NUM_STRS 3

const char *filename = "/tmp/test116.tpl";

int main() {
  tpl_node *tn;
  int i,d=1,D=-1;
  char c='a', C='0';
  char *strs[NUM_STRS] = {"alpha", "beta", "gamma"};
  char *STRS[NUM_STRS] = {"femto", "nano", "centi"};

  tn = tpl_map("cs#i", &c, strs, NUM_STRS, &d);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("cs#i", &C, STRS, NUM_STRS, &D);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  printf("%d %c\n", D, C);
  for(i=0;i<NUM_STRS;i++) printf("%s\n", STRS[i]);

  return 0;
}
