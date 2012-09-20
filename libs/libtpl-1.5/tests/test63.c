#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i;

    tn = tpl_map("A(i)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);

    /* test pack-then-unpack without dump/load; implicit dump/load*/
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);

    /* implicit conversion back to output tpl (discards previous data in tpl */
    for(i=0;i>-10;i--) tpl_pack(tn,1);

    /* one more implicit conversion */
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);

    tpl_free(tn);
    return(0);
}
