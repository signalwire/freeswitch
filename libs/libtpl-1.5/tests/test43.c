#include "tpl.h"

int main(int argc, char *argv[]) {
    tpl_node *tn;
    char id;
    tpl_bin bin;

    char *junk = "0123456789";
    bin.sz = 10;
    bin.addr = junk;

    tn = tpl_map("A(cB)", &id, &bin);

    for(id=0; id < 3; ++id) 
        tpl_pack(tn,1);  /* pack same bin buffer, doesn't matter */

    tpl_dump(tn, TPL_FILE, "/tmp/test43.tpl");
    tpl_free(tn);
    return(0);
}
