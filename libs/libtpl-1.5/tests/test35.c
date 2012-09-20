#include "tpl.h"

int main(int argc, char *argv[]) {
    tpl_node *tn;
    int id;
    char *name, *names[] = { "joe", "bob", "cary" };

    tn = tpl_map("A(is)", &id, &name);

    for(id=0,name=names[id]; id < 3; name=names[++id])
        tpl_pack(tn,1);

    tpl_dump(tn, TPL_FILE, "/tmp/test35.tpl");
    tpl_free(tn);
    return(0);
}
