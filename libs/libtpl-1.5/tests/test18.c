#include <stdio.h>
#include "tpl.h"

extern tpl_hook_t tpl_hook;

int main() {
    tpl_node *tn;
    int i=-1;

    tpl_hook.oops = printf;

    tn = tpl_map("A(i",&i);
    printf("tpl map %s\n", tn ? "succeeded" : "failed");
    return(0);
}
