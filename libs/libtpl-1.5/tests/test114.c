#include <stdio.h>
#include <string.h>
#include "tpl.h"

#define XDIM 10
#define YDIM 2

const char *filename = "/tmp/test114.tpl";
extern tpl_hook_t tpl_hook;

int main() {
  tpl_node *tn;
  int xy[XDIM][YDIM], XY[XDIM][YDIM];
  int i,j;

  tpl_hook.oops = printf;

  for(i=0; i<XDIM; i++) {
    for(j=0; j<YDIM; j++) {
      xy[i][j] = i+j;
      XY[i][j] = 0;
    }
  }

  tn = tpl_map("i##", xy, XDIM, YDIM);
  if (!tn) {
    printf("tpl_map failed; exiting\n");
    return -1;
  }
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("i##", XY, XDIM, YDIM);
  if (!tn) {
    printf("tpl_map failed; exiting\n");
    return -1;
  }
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  printf("matrices %s\n", (!memcmp(xy,XY,sizeof(xy))) ?  "match" : "mismatch");
  return 0;
}
