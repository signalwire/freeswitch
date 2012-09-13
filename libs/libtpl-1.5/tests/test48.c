#include <stdio.h>
#include "tpl.h"

int main() {
    tpl_node *tn;
    int i;

    tn = tpl_map( "A(i)", &i );
    tpl_load( tn, TPL_FILE, "/tmp/test47.tpl" );
    while (tpl_unpack( tn, 1 ) > 0) {
        printf("%d ", i);
    }
    tpl_free( tn );
    return(0);
}
