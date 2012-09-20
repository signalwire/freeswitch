#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpl.h"

struct ms_t {
    char c;
    int i;
    char s[2];
    char o;
    char *x;
    char y[3];
    double d;
};

int main() {
    tpl_node *tn;
    struct ms_t ms = {/*.c =*/ 'a', /*.i =*/ 1, /*.s =*/ {'h','i'}, /*.o =*/ 'o', /*.x =*/ "matilda", /*.y =*/ {'t','d','h'}, /*.d =*/ 3.14 };
    struct ms_t ms2;

    tn = tpl_map("S(cic#csc#f)", &ms, 2, 3);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test73.tpl");
    tpl_free(tn);

    memset(&ms2,0,sizeof(struct ms_t));
    tn = tpl_map("S(cic#csc#f)", &ms2, 2, 3);
    tpl_load(tn,TPL_FILE,"/tmp/test73.tpl");
    tpl_unpack(tn,0);
    tpl_free(tn);
    printf("%c\n%d\n%c%c\n%c\n%s\n%c%c%c\n%f\n", ms2.c, ms2.i, ms2.s[0], ms2.s[1], ms2.o, ms2.x, ms2.y[0],ms2.y[1],ms2.y[2], ms2.d);
    free(ms2.x);
    return(0);
}
