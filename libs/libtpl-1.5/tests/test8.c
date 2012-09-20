#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    unsigned int i;
    unsigned char b,c;
    unsigned char d,e;

    tn = tpl_map("cA(c)",&b,&c);
    b = 255;
    tpl_pack(tn,0);
    for (i=0; i < 10; i++) {
        c = 'a' + i;
        tpl_pack(tn,1);
    }
    tpl_dump(tn,TPL_FILE,"/tmp/test8.tpl");
    tpl_free(tn);

    tn = tpl_map("cA(c)",&d,&e);
    tpl_load(tn,TPL_FILE,"/tmp/test8.tpl");
    tpl_unpack(tn,0);
    while (tpl_unpack(tn,1) > 0) 
        printf("d is %x, e is %x\n", (unsigned)d, (unsigned)e);
    tpl_free(tn);
    return(0);
}
