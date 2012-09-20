#include <stdio.h>
#include "tpl.h"

extern tpl_hook_t tpl_hook;

int main() {
    tpl_node *tn;
    int i;

    tpl_hook.oops = printf;

    tn = tpl_map("A(c)",&i);
    tpl_load(tn,TPL_FILE,"test17.tpl");
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);
    tpl_free(tn);
    return(0);
}
