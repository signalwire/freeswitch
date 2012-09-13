#include "tpl.h"

int main() {
    tpl_node *tn;
    int i,j;

    tn = tpl_map("A(A(i))",&j);

    for(i=2;i<4;i++) {

        for(j=i; j < 10*i; j *= i) {
            tpl_pack(tn,2);
        }
        tpl_pack(tn,1);
    }

    tpl_dump(tn, TPL_FILE, "/tmp/test37.tpl");
    tpl_free(tn);
    return(0);
}
