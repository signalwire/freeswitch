#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i;

    tn = tpl_map("A(i)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE,"/tmp/test60.tpl");

    /* test load-after-pack: implicit free via tpl_free_keep_map() */
    tpl_load(tn,TPL_FILE,"/tmp/test60.tpl");
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);

    tpl_free(tn);
    return(0);
}
