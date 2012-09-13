#include "tpl.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

typedef struct {
  int64_t j;
  int l1;
  unsigned l2;
  int i;
  int16_t h;
  char c[4];
} test_t;

const char *filename = "/tmp/test108.tpl";

int main() {
  test_t s[5], t[5];
  tpl_node *tn;
  int w=10,x=20,y=30,z=40,W,X,Y,Z;
  uint64_t b=10,B;

  memset(s, 0, sizeof(s));
  memset(t, 0, sizeof(t));

  s[0].j=0;   s[0].i=0; s[0].l1= 0; s[0].l2=0;  s[0].h=   0; strcpy(s[0].c, "cat");
  s[1].j=100; s[1].i=1; s[1].l1=-1; s[1].l2=10; s[1].h=1000; strcpy(s[1].c, "dog");
  s[2].j=200; s[2].i=2; s[2].l1=-2; s[2].l2=20; s[2].h=2000; strcpy(s[2].c, "eel");
  s[3].j=300; s[3].i=3; s[3].l1=-3; s[3].l2=30; s[3].h=3000; strcpy(s[3].c, "emu");
  s[4].j=400; s[4].i=4; s[4].l1=-4; s[4].l2=40; s[4].h=4000; strcpy(s[4].c, "ant");

  tn = tpl_map("IS(Iiuijc#)#iiii", &b, s, 4, 5, &w, &x, &y, &z);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("IS(Iiuijc#)#iiii", &B, t, 4, 5, &W, &X, &Y, &Z);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  if (memcmp(t,s,sizeof(t)) == 0) printf("structure matches original\n");
  else printf("structure mismatches original\n");

  if (b==B && w==W && x==X && y==Y && z==Z) printf("other fields match original\n");
  else printf("other fields mismatch originals\n");

  return 0;
}
