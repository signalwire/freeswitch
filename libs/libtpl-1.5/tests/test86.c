#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int o,i;
    void *addr;
    int sz;
    char *fmt;

    tn = tpl_map("A(A(i))",&i);
    for(o=0;o<10;o++) {
        for(i=o; i < o+10; i++) tpl_pack(tn,2);
        tpl_pack(tn,1);
    }
    tpl_dump(tn,TPL_MEM,&addr,&sz);
    tpl_free(tn);

    fmt = tpl_peek(TPL_MEM, addr, sz);
    if (fmt) {
        printf("%s\n",fmt);
        free(fmt);
    }
    free(addr);
    return(0);
}
