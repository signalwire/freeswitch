#include "tpl.h"

int main() {
    char c;
    tpl_node *tn;

    tn = tpl_map("A(A(c))", &c);

    for(c='a'; c<'c'; c++) tpl_pack(tn,2);
    tpl_pack(tn, 1);

    for(c='1'; c<'4'; c++) tpl_pack(tn,2);
    tpl_pack(tn, 1);

    tpl_dump(tn, TPL_FILE, "/tmp/test40.tpl");
    tpl_free(tn);
    return(0);
}
