#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    double x;

    tn = tpl_map("A(f)",&x);
    for(x=0.0;x<1.0;x+=0.2) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE,"/tmp/test46.tpl");
    tpl_free(tn);

    return(0);
}
