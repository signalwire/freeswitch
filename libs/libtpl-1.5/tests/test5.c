#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int o,i,j=-1;
    void *addr;
    int sz;

    tn = tpl_map("A(A(i))",&i);
    for(o=0;o<10;o++) {
        for(i=o; i < o+10; i++) tpl_pack(tn,2);
        tpl_pack(tn,1);
    }
    tpl_dump(tn,TPL_MEM,&addr,&sz);
    tpl_free(tn);

    tn = tpl_map("A(A(i))",&j);
    tpl_load(tn,TPL_MEM,addr,sz);
    while (tpl_unpack(tn,1) > 0) {
        while (tpl_unpack(tn,2) > 0) printf("j is %d\n", j);
    }
    tpl_free(tn);
    free(addr);
    return(0);
}
