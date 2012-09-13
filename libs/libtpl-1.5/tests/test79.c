#include <stdio.h>
#include "tpl.h"

int main() {
    int i;
    char c;
    tpl_node *tn;

    tn = tpl_map("A(i)c", &i, &c);
    tpl_load(tn, TPL_FILE, "/tmp/test78.tpl");

    /* unpack index number 0 (char c) */
    tpl_unpack(tn, 0);
    printf("got %c\n", c);

    /* unpack A(i) (that is, index number 1) til we run out of elements */
    while (tpl_unpack(tn, 1) > 0) {
        printf("got %d\n", i);
    }
    tpl_free(tn);
    return(0);
}
