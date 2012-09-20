#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "tpl.h"

const char *filename = "/tmp/test112.tpl";

#define NUM 10

struct st {
  int i;
  char c[8];
  double f;
  uint16_t v[2];
};


int main() {
  struct st s[NUM], d[NUM];
  tpl_node *tn; 
  int i;

  memset(s, 0, sizeof(s)); /* clear s */
  memset(d, 0, sizeof(d)); /* clear d */

  /* fill s with random stuff */
  for(i=0; i < NUM; i++) {
    s[i].i = i; s[i].f = 3.14159 * i; s[i].v[0] = NUM*i; s[i].v[1] = NUM+i; 
    strncpy(s[i].c, "abcdefg",8);
    s[i].c[0] += 1;
  }

  tn = tpl_map("S(ic#fv#)#", s, 8, 2, NUM);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("S(ic#fv#)#", d, 8, 2, NUM);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  /* see if the result is the same as the s */
  printf("structures %s\n", (!memcmp(d,s,sizeof(d)))? "match" : "mismatch");
  return 0;
}

