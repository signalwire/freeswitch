#include "tpl.h"

int main(int argc, char *argv[]) {
    tpl_node *tn;
    char id;
    char *name, *names[] = { "joe", "bob", "cary" };

    tn = tpl_map("A(cs)", &id, &name);

    for(id=0,name=names[(int)id]; id < 3; name=names[(int)++id])
        tpl_pack(tn,1);

    tpl_dump(tn, TPL_FILE, "/tmp/test42.tpl");
    tpl_free(tn);
    return(0);
}
