#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i,j=-1;

    tn = tpl_map("A(i)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE,"/tmp/test3.tpl");
    tpl_free(tn);

    tn = tpl_map("A(i)",&j);
    tpl_load(tn,TPL_FILE,"/tmp/test3.tpl");
    while (tpl_unpack(tn,1) > 0) printf("j is %d\n", j);
    tpl_free(tn);
    return(0);
}
