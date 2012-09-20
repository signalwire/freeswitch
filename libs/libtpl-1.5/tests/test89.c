#include "tpl.h"
#include <stdio.h>

struct ms_t {
    int i;
    char c[3];
    double f;
};

int main() {
    tpl_node *tn;
    struct ms_t ms;

    tn = tpl_map( "S(ic#f)", &ms, 3);
    tpl_load( tn, TPL_FILE, "/tmp/test88.tpl" );
    tpl_unpack( tn, 0 );
    tpl_free( tn );

    printf("%d\n%c%c%c\n%.2f\n", ms.i, ms.c[0],ms.c[1],ms.c[2], ms.f);
    return(0);
}
