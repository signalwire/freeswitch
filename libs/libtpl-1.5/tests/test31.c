#include "tpl.h"
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char*argv[]) {
    tpl_bin bin;
    tpl_node *tn;
    int i;
    char *file = "/tmp/test31.tpl";

    bin.addr = NULL;
    bin.sz = 0;  

    tn = tpl_map("B", &bin);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,file);
    tpl_free(tn);

    /* load these two fields with bogus values to test that tpl_unpack
     * sets them back to NULL, and 0 respectively.  */
    bin.addr = file;  
    bin.sz = 4;      

    tn = tpl_map("B", &bin);
    tpl_load(tn,TPL_FILE,file);
    tpl_unpack(tn,0);
    tpl_free(tn);

    /* print the buffer char-by-char ; its not a nul-termd string */
    printf("buffer length: %u\n", bin.sz);
    for(i=0; i < bin.sz; i++) printf("%c", ((char*)bin.addr)[i]);
    printf("\n");

    if (bin.sz > 0) 
        free(bin.addr);  /* malloc'd for us by tpl_unpack, we must free */
    return(0);
}

