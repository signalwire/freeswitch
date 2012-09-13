#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpl.h"

#define NUM_STRS 3
#define NUM_ELMT 3
#define STR "apple"
#define SLEN 5

const char *filename = "/tmp/test117.tpl";

int main() {
  tpl_node *tn;
  int i,j,d=1,D=-1;
  char c='a', C='0';
  char *strs[NUM_STRS];
  char *STRS[NUM_STRS];

  tn = tpl_map("cA(s#)i", &c, strs, NUM_STRS, &d);
  for(i=0; i<NUM_ELMT; i++) {  /* pack the same thing this many times*/
    for(j=0; j<NUM_STRS; j++) {/* each time just tweaking them a bit */
      strs[j] = malloc( SLEN+1 );
      memcpy(strs[j], STR, SLEN+1);
      strs[j][0] = 'a'+j;
    }
    tpl_pack(tn,1);
  }
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("cA(s#)i", &C, STRS, NUM_STRS, &D);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  while(tpl_unpack(tn,1)>0) {
    for(i=0;i<NUM_STRS;i++) {
      printf("%s\n", STRS[i]);
      free(STRS[i]);
    }
  }
  tpl_free(tn);

  printf("%d %c\n", D, C);

  return 0;
}
