#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    double x,y;

    printf("sizeof(double) is %d\n", (int)sizeof(double));

    tn = tpl_map("A(f)",&x);
    for( x=1.0; x < 10.0; x += 2/3.0) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE,"/tmp/test34.tpl");
    tpl_free(tn);

    tn = tpl_map("A(f)",&y);
    tpl_load(tn,TPL_FILE,"/tmp/test34.tpl");
    while (tpl_unpack(tn,1) > 0) printf("y is %.6f\n", y);
    tpl_free(tn);
    
    return(0);
}
