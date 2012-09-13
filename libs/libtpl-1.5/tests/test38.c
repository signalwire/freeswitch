#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int j;

    tn = tpl_map("A(A(i))",&j);
    tpl_load(tn,TPL_FILE, "/tmp/test37.tpl");

    while (tpl_unpack(tn,1) > 0) {
        printf("unpacking index 1:\n");
        while (tpl_unpack(tn,2) > 0) {
            printf("   unpacking index 2: j is %d\n", j);
        }
    }
    tpl_free(tn);
    return(0);
}
