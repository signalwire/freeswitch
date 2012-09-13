#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tpl.h"
#include <inttypes.h>

const char *filename = "/tmp/test110.tpl";

int nstrcmp(const char *a, const char *b) {
  if (a==NULL || b==NULL) return (a==NULL && b==NULL)?0:1;
  return strcmp(a,b);
}

int main() {
  tpl_node *tn;

  char *s1,*s2,*s3,*s4;
  int i5,i6,i7,i8,i9,
      i10,i11,i12,i13,i14,i15,i16,i17,i18,i19,i20,
      i21,i22,i23,i24,i25,i26,i27,i28,i29,i30,i31,i32,i33;
  double f34,f35,f36;
  int i37,i38,i39,i40;

  char *S1,*S2,*S3,*S4;
  int I5,I6,I7,I8,I9,
      I10,I11,I12,I13,I14,I15,I16,I17,I18,I19,I20,
      I21,I22,I23,I24,I25,I26,I27,I28,I29,I30,I31,I32,I33;
  double F34,F35,F36;
  int I37,I38,I39,I40;

  s1=NULL;s2=NULL;s3="testing";s4="some_string";
  i5=5;i6=6;i7=7;i8=8;i9=9;
  i10=10;i11=11;i12=12;i13=13;i14=14;i15=15;i16=16;i17=17;i18=18;i19=19;i20=20;
  i21=21;i22=22;i23=23;i24=24;i25=25;i26=26;i27=27;i28=28;i29=29;i30=30;i31=31;
  i32=32;i33=33;
  f34=34.0;f35=35.0;f36=36.0;
  i37=37;i38=38;i39=39;i40=40;


  tn = tpl_map("ssssiiiiiiiiiiiiiiiiiiiiiiiiiiiiifffiiii", 
              &s1,&s2,&s3,&s4,
              &i5,&i6,&i7,&i8,&i9,
              &i10,&i11,&i12,&i13,&i14,&i15,&i16,&i17,&i18,&i19,&i20,
              &i21,&i22,&i23,&i24,&i25,&i26,&i27,&i28,&i29,&i30,&i31,&i32,&i33,
              &f34,&f35,&f36,
              &i37,&i38,&i39,&i40);
  tpl_pack(tn,0);
  tpl_dump(tn,TPL_FILE,filename);
  tpl_free(tn);

  tn = tpl_map("ssssiiiiiiiiiiiiiiiiiiiiiiiiiiiiifffiiii", 
              &S1,&S2,&S3,&S4,
              &I5,&I6,&I7,&I8,&I9,
              &I10,&I11,&I12,&I13,&I14,&I15,&I16,&I17,&I18,&I19,&I20,
              &I21,&I22,&I23,&I24,&I25,&I26,&I27,&I28,&I29,&I30,&I31,&I32,&I33,
              &F34,&F35,&F36,
              &I37,&I38,&I39,&I40);
  tpl_load(tn,TPL_FILE,filename);
  tpl_unpack(tn,0);
  tpl_free(tn);

  if (
    !nstrcmp(s1,S1) && !nstrcmp(s2,S2) && !nstrcmp(s3,S3) && !nstrcmp(s4,S4) &&
    i5==I5 && i6==I6 && i7==I7 && i8==I8 && i9==I9 && i10==I10 && i11==I11 &&
    i12==I12 && i13==I13 && i14==I14 && i15==I15 && i16==I16 && i17==I17 &&
    i18==I18 && i19==I19 && i20==I20 && i21==I21 && i22==I22 && i23==I23 &&
    i24==I24 && i25==I25 && i26==I26 && i27==I27 && i28==I28 && i29==I29 &&
    i30==I30 && i31==I31 && i32==I32 && i33==I33 && f34==F34 && f35==F35 &&
    f36==F36 && i37==I37 && i38==I38 && i39==I39 && i40==I40
    ) {
    printf("structures match\n");
    free(S1); free(S2); free(S3); free(S4);
  }
  else
    printf("structures mismatch\n");

  return 0;
}
