#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    unsigned i;

    tn = tpl_map("A(u)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE,"/tmp/test22.tpl");
    tpl_free(tn);

    tn = tpl_map("A(u)",&i);
    tpl_load(tn,TPL_FILE,"/tmp/test22.tpl");
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);
    tpl_free(tn);
    return(0);
}
