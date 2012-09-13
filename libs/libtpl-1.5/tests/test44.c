#include "tpl.h"

int main(int argc, char *argv[]) {
    tpl_node *tn;
    char id,j;
    tpl_bin bin;

    char *junk = "0123456789";
    bin.sz = 10;
    bin.addr = junk;

    tn = tpl_map("A(cA(B))", &id, &bin);

    for(id=0; id < 3; ++id) {
        for(j=0;j<2;j++) 
            tpl_pack(tn,2); /* pack same bin buffer, doesn't matter */
        tpl_pack(tn,1);  
    }

    tpl_dump(tn, TPL_FILE, "/tmp/test44.tpl");
    tpl_free(tn);
    return(0);
}
