#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i,rc,j;
    char toosmall[10];
    char buf[60];

    tn = tpl_map("A(i)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);
    rc=tpl_dump(tn,TPL_MEM|TPL_PREALLOCD,toosmall,sizeof(toosmall));
    printf("testing undersized output buffer... %d \n", rc);
    rc=tpl_dump(tn,TPL_MEM|TPL_PREALLOCD,buf,sizeof(buf));
    printf("testing sufficient output buffer... %d \n", rc);
    tpl_free(tn);

    tn = tpl_map("A(i)",&j);
    tpl_load(tn,TPL_MEM|TPL_EXCESS_OK,buf,sizeof(buf)); 
    while (tpl_unpack(tn,1) > 0) printf("j is %d\n", j);
    tpl_free(tn);
    return(0);
}
