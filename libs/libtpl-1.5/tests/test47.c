#include "tpl.h"

int main() {
    tpl_node *tn;
    int i;

    tn = tpl_map( "A(i)", &i );
    for( i=0; i<10; i++ ) {
        tpl_pack( tn, 1 );  
    }
    tpl_dump( tn, TPL_FILE, "/tmp/test47.tpl" );
    tpl_free( tn );
    return(0);
}
