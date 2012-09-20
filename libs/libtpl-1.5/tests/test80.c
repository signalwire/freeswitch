#include <stdio.h>
#include "tpl.h"

#define ILEN 10

struct st {
    char c;
    int i[ILEN];
};

int main() {
    struct st dst, s = {'a', {0,1,2,3,4,5,6,7,8,9}};
    tpl_node *tn;
    int j;

    tn = tpl_map("A(S(ci#))", &s, ILEN);
    tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE,"/tmp/test80.tpl");
    tpl_free(tn);

    tn = tpl_map("A(S(ci#))", &dst, ILEN);
    tpl_load(tn,TPL_FILE,"/tmp/test80.tpl");
    while (tpl_unpack(tn,1) > 0) {
        printf("%c ", dst.c);
        for(j=0;j<ILEN;j++) printf("%d ", dst.i[j]);
        printf("\n");
    }
    tpl_free(tn);
    return(0);
}
