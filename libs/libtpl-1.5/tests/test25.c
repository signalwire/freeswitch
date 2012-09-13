#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern tpl_hook_t tpl_hook;

int main() {
    tpl_node *tn;
    unsigned i;
    char *file = "test25.tpl";

    tpl_hook.oops = printf;

    tn = tpl_map("A(u)",&i);
    if (tpl_load(tn, TPL_FILE, file) < 0 ) {
        tpl_free(tn);
        exit(-1);
    }
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);
    tpl_free(tn);
    return(0);
}
