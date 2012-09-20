#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

#define S2_LEN 10

struct example {
    char *s1;         /* s1 is a pointer */
    char s2[S2_LEN];  /* s2 is a byte array */
};

int main() {
    tpl_node *tn;
    int i;
    struct example dst, src = {
        /* .s1 = */ "string",
        /* .s2 = */ {'b','y','t','e',' ','a','r','r','a','y'}
    };

    tn = tpl_map( "S(sc#)", &src, S2_LEN);   /* NOTE S(...) */
    tpl_pack( tn, 0 );
    tpl_dump( tn, TPL_FILE, "/tmp/test76.tpl" );
    tpl_free( tn );

    /* unpack it now into another struct */

    tn = tpl_map( "S(sc#)", &dst, S2_LEN);   /* NOTE S(...) */
    tpl_load( tn, TPL_FILE, "/tmp/test76.tpl" );
    tpl_unpack( tn, 0 );
    tpl_free( tn );

    printf("%s\n", dst.s1);
    for(i=0; i < S2_LEN; i++) printf("%c", dst.s2[i]);
    printf("\n");

    free(dst.s1);   /* tpl allocated it for us; we must free it */
    return(0);
}
