#include <stdio.h>
#include "tpl.h"

extern tpl_hook_t tpl_hook;

int main() {
    tpl_node *tn;
    int i, rc;

    tpl_hook.oops = printf;

    tn = tpl_map("A(i)",&i);
    rc = tpl_load(tn,TPL_FILE,"test39.tpl");
    printf("load %s (rc=%d)\n", (rc >= 0 ? "ok" : "failed"), rc);
    tpl_free(tn);
    return(0);
}
