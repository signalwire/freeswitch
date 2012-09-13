#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    double x=0.5,y=0.0;

    tn = tpl_map("f",&x);
    tpl_pack(tn,0);
    printf("x is %f\n", x);
    tpl_dump(tn,TPL_FILE,"/tmp/test45.tpl");
    tpl_free(tn);

    tn = tpl_map("f",&y);
    tpl_load(tn,TPL_FILE,"/tmp/test45.tpl");
    tpl_unpack(tn,0);
    printf("y is %f\n", y);
    tpl_free(tn);
    return(0);
    
}
