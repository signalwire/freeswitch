#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    char *s,*t;
    void *addr;
    int sz;

    tn = tpl_map("s",&s);
    s = "hello, world!";
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_MEM,&addr,&sz);
    tpl_free(tn);

    tn = tpl_map("s",&t);
    tpl_load(tn,TPL_MEM,addr,sz);
    tpl_unpack(tn,0);
    printf("t is %s\n", t);
    free(t);
    tpl_free(tn);
    free(addr);
    return(0);
}
