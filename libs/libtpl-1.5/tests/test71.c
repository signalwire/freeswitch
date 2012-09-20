#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpl.h"

struct ms_t {
    int i;
    char s[2];
    char o;
    char *x;
    char y;
    double d;
};

int main() {
    tpl_node *tn;
    struct ms_t ms = { /*.i =*/ 1, /*.s =*/ {'h','i'}, /*.o =*/ 'o', /*.x =*/ "matilda", /*.y =*/ 'y', /*.d =*/ 3.14 };
    struct ms_t ms2;
    char b = '#', b2;

    tn = tpl_map("cS(ic#cscf)", &b, &ms, 2);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test71.tpl");
    tpl_free(tn);

    memset(&ms2,0,sizeof(struct ms_t));
    tn = tpl_map("cS(ic#cscf)", &b2, &ms2, 2);
    tpl_load(tn,TPL_FILE,"/tmp/test71.tpl");
    tpl_unpack(tn,0);
    tpl_free(tn);
    printf("%c\n%d\n%c%c\n%c\n%s\n%c\n%f\n", b2, ms2.i, ms2.s[0], ms2.s[1], ms2.o, ms2.x, ms2.y, ms2.d);
    free(ms2.x);
    return(0);
}
