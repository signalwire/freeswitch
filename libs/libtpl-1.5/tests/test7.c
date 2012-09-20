#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    char *strs[] = { "wonderful", "prince of peace", "counselor", NULL };
    int i;
    char *s, *t;

    tn = tpl_map("A(s)",&s);
    for(i=0; strs[i] != NULL; i++) {
        s = strs[i];
        tpl_pack(tn,1);
    }
    tpl_dump(tn,TPL_FILE,"/tmp/test7.tpl");
    tpl_free(tn);

    tn = tpl_map("A(s)",&t);
    tpl_load(tn,TPL_FILE,"/tmp/test7.tpl");
    while (tpl_unpack(tn,1) > 0) {
        printf("t is %s\n", t);
        free(t);
    }
    tpl_free(tn);
    return(0);
}
