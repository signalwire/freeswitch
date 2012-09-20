#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i,j=-1;

    tn = tpl_map("A(ii)",&i,&j);
    for(i=0,j=1;i<10;i+=2,j+=2) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE,"/tmp/test21.tpl");
    tpl_free(tn);

    tn = tpl_map("A(ii)",&i,&j);
    tpl_load(tn,TPL_FILE,"/tmp/test21.tpl");
    while (tpl_unpack(tn,1) > 0) printf("i,j are %d,%d\n", i,j);
    tpl_free(tn);
    return(0);
}
