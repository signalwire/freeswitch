#include "tpl.h"

struct ms_t {
    int i;
    char c[3];
    double f;
};

int main() {
    tpl_node *tn;
    struct ms_t ms = {1, {'a','b','c'}, 3.14};

    tn = tpl_map( "S(ic#f)", &ms, 3);
    tpl_pack( tn, 0 );
    tpl_dump( tn, TPL_FILE, "/tmp/test88.tpl" );
    tpl_free( tn );
    return(0);
}
