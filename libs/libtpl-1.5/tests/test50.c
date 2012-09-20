    #include <stdio.h>
    #include <stdlib.h>
    #include "tpl.h"

    int main() {
        tpl_node *tn;
        char *s;

        tn = tpl_map( "A(s)", &s );
        tpl_load( tn, TPL_FILE, "/tmp/test49.tpl" );

        while (tpl_unpack( tn, 1 ) > 0) {
            printf("%s\n", s);
            free(s);  /* important! */
        }

        tpl_free(tn);
        return(0);
    }
