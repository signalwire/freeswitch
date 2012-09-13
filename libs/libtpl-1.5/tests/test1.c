#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i,j=-1;
    void *addr;
    size_t sz;

    tn = tpl_map("i",&i);
    i=1;
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_MEM,&addr,&sz);
    tpl_free(tn);

    tn = tpl_map("i",&j);
    tpl_load(tn,TPL_MEM,addr,sz);
    tpl_unpack(tn,0);
    printf("j is %d\n", j);
    tpl_free(tn);
    free(addr);
    return(0);
}
