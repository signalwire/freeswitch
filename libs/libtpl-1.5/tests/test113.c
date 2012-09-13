#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "tpl.h"

const char *filename = "/tmp/test113.tpl";

#define NUM 10

struct st {
  int i;
  char c[8];
  double f;
  uint16_t v[2];
};


int main() {
  struct st s[NUM], t[NUM];
  tpl_node *tn; 
  int i;
  int a=5,d=8, A, D;
  char b='6',c='7', B, C;

  memset(s, 0, sizeof(s)); /* clear s */
  memset(t, 0, sizeof(t)); /* clear t */

  /* fill s with random stuff */
  for(i=0; i < NUM; i++) {
    s[i].i = i; s[i].f = 3.14159 * i; s[i].v[0] = NUM*i; s[i].v[1] = NUM+i; 
    strncpy(s[i].c, "abcdefg",8);
    s[i].c[0] += 1;
  }

  tn = tpl_map("icS(ic#fv#)#ci", &a, &b, s, 8, 2, NUM, &c, &d);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("icS(ic#fv#)#ci", &A, &B, t, 8, 2, NUM, &C, &D);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  /* see if the result is the same as the s */
  printf("structures %s\n", (!memcmp(t,s,sizeof(d)))? "match" : "mismatch");
  printf("A %s\n", (a==A)? "matches" : "mismatches");
  printf("B %s\n", (b==B)? "matches" : "mismatches");
  printf("C %s\n", (c==C)? "matches" : "mismatches");
  printf("D %s\n", (d==D)? "matches" : "mismatches");
  return 0;
}

