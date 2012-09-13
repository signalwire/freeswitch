#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i,j=-1;

    tn = tpl_map("i",&i);
    i=1;
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test2.tpl");
    tpl_free(tn);

    tn = tpl_map("i",&j);
    tpl_load(tn,TPL_FILE,"/tmp/test2.tpl");
    tpl_unpack(tn,0);
    printf("j is %d\n", j);
    tpl_free(tn);
    return(0);
    
}
