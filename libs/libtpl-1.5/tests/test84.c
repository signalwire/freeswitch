#include <stdio.h>
#include "tpl.h"

#define ILEN 10
#define KLEN 8
#define FLEN 5

struct st {
    char c;
    double f[FLEN];
};

int main() {
    tpl_node *tn;
    /* some meaningless test data */
    struct st s;
    int j; 
    int i[ILEN];
    int k[KLEN];
    char a;
    char b;

    tn = tpl_map("cA(i#)S(cf#)A(ci#)", &a, i, ILEN, &s, FLEN, &b, k, KLEN);
    tpl_load(tn,TPL_FILE,"test84_0.tpl");

    tpl_unpack(tn,0);
    printf("%c %c %.2f %.2f %.2f %.2f %.2f \n", a, s.c, s.f[0], s.f[1], s.f[2], s.f[3], s.f[4]);

    while( tpl_unpack(tn,1) > 0) {
        for(j=0;j<ILEN;j++) printf("%d ", i[j]);
        printf("\n");
    }

    while( tpl_unpack(tn,2) > 0) {
        printf("%c ", b);
        for(j=0;j<KLEN;j++) printf("%d ", k[j]);
        printf("\n");
    }

    tpl_free(tn);

    /* use the big-endian input file and repeat */

    tn = tpl_map("cA(i#)S(cf#)A(ci#)", &a, i, ILEN, &s, FLEN, &b, k, KLEN);
    tpl_load(tn,TPL_FILE,"test84_1.tpl");

    tpl_unpack(tn,0);
    printf("%c %c %.2f %.2f %.2f %.2f %.2f \n", a, s.c, s.f[0], s.f[1], s.f[2], s.f[3], s.f[4]);

    while( tpl_unpack(tn,1) > 0) {
        for(j=0;j<ILEN;j++) printf("%d ", i[j]);
        printf("\n");
    }

    while( tpl_unpack(tn,2) > 0) {
        printf("%c ", b);
        for(j=0;j<KLEN;j++) printf("%d ", k[j]);
        printf("\n");
    }

    tpl_free(tn);
    return(0);
}
