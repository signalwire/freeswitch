#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

const char *filename = "/tmp/test121.tpl";
int main() {
  char *labels[2][3] = { {"one", "two", "three"},
                         {"eins", "zwei", "drei" } };
  char *olabels[2][3] = { {NULL,NULL,NULL }, {NULL,NULL,NULL}};
  int i,j;

  tpl_node *tn;
  tn = tpl_map("s##", labels, 2, 3);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("s##", olabels, 2, 3);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  for(i=0;i<2;i++) {
    for(j=0;j<3;j++) {
      printf("%s\n", olabels[i][j]);
      free(olabels[i][j]);
    }
  }

  return 0;
}
