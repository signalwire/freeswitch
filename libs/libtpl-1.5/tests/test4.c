#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i,j=-1;
    void *addr;
    int sz;

    tn = tpl_map("A(i)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);
    tpl_dump(tn,TPL_MEM,&addr,&sz);
    tpl_free(tn);

    tn = tpl_map("A(i)",&j);
    tpl_load(tn,TPL_MEM,addr,sz);
    while (tpl_unpack(tn,1) > 0) printf("j is %d\n", j);
    tpl_free(tn);
    free(addr);
    return(0);
}
