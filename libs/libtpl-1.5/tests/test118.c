#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

#define ILEN 10
#define KLEN 8
#define FLEN 5

struct st {
    char c;
    double f[FLEN];
};

const char *filename = "/tmp/test118.tpl";

int main() {
    tpl_node *tn;
    /* some meaningless test data */
    struct st s = {'z', {0.9, 0.8, 0.7, 0.6, 0.5 }};
    int j; 
    int i[ILEN] = {-1, -2, -3, -4, -5, -6, -7, -8, -9, -10};
    int k[KLEN] = {100, 200, 300, 400, 500, 600, 700, 800};
    char a = '&';
    char b = 'x';
    const char *fmt;
    uint32_t num_fxlens, *fxlens;

    tn = tpl_map("cA(i#)S(cf#)A(ci#)", &a, i, ILEN, &s, FLEN, &b, k, KLEN);
    tpl_pack(tn,0);

    tpl_pack(tn,1);
    for(j=0; j < ILEN; j++) i[j]--;
    tpl_pack(tn,1);
    for(j=0; j < ILEN; j++) i[j]--;
    tpl_pack(tn,1);

    tpl_pack(tn,2);
    b++;
    for(j=0; j < KLEN; j++) k[j] += 50;  
    tpl_pack(tn,2);
    b++;
    for(j=0; j < KLEN; j++) k[j] += 50; 
    tpl_pack(tn,2);

    tpl_dump(tn,TPL_FILE,filename);
    tpl_free(tn);

    /* now peek at the fxlens */
    fmt = tpl_peek(TPL_FILE|TPL_FXLENS, filename, &num_fxlens, &fxlens);
    printf("format %s\n", fmt);
    printf("num_fxlens %u\n", num_fxlens);
    for(j=0; j<num_fxlens; j++) printf("fxlens[%u] %u\n", j, fxlens[j]);
    if (num_fxlens>0) free(fxlens);
    return(0);
}
