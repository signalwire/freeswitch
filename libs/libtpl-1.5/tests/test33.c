#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    double x,y;

    printf("sizeof(double) is %d\n", (int)sizeof(double));

    tn = tpl_map("f",&x);
    x=1.0;
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test33.tpl");
    tpl_free(tn);

    tn = tpl_map("f",&y);
    tpl_load(tn,TPL_FILE,"/tmp/test33.tpl");
    tpl_unpack(tn,0);
    printf("y is %.6f\n", y);
    tpl_free(tn);
    
    return(0);
}
