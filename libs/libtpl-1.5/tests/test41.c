#include "tpl.h"
#include <stdio.h>

int main() {
    char c;
    tpl_node *tn;

    tn = tpl_map("A(A(c))", &c);

    tpl_load(tn, TPL_FILE, "/tmp/test40.tpl");
    while (tpl_unpack(tn,1) > 0) {
        while (tpl_unpack(tn,2) > 0) printf("%c ",c);
        printf("\n");
    }
    tpl_free(tn);
    return(0);
}

