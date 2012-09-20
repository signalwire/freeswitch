#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i;

    tn = tpl_map("A(i)",&i);
    tpl_load(tn,TPL_FILE,"test61_0.tpl");
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);

    /* test load-then-load: implicit free via tpl_free_keep_map */
    tpl_load(tn, TPL_FILE,"test61_1.tpl");
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);

    tpl_free(tn);
    return(0);
}
