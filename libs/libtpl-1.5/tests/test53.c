#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int j;

    tn = tpl_map("A(A(i))",&j);

    j=1;
    tpl_pack(tn,2);
    tpl_pack(tn,1);
    /* j=2; */
    /* tpl_pack(tn,2); */
    tpl_pack(tn,1);  /* pack zero-length nested array */

    tpl_dump(tn, TPL_FILE, "/tmp/test53.tpl");
    tpl_free(tn);

    tn = tpl_map("A(A(i))",&j);
    tpl_load(tn, TPL_FILE, "/tmp/test53.tpl");
    while(tpl_unpack(tn,1) > 0) {
        printf("----------\n");
        while(tpl_unpack(tn,2) > 0) {
            printf("--> j is %d\n", j);
        }
    }
    tpl_free(tn);
    return(0);
}
