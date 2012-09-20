#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

#define TEST_LEN1 10
#define TEST_LEN2 5

extern tpl_hook_t tpl_hook;

int main() {
    tpl_node *tn;
    int i[TEST_LEN1] = {1,2,3,4,5,6,7,8,9,10};
    int j[TEST_LEN2] = {5,4,3,2,1};
    int i2[TEST_LEN1];
    int j2[TEST_LEN2];
    int x;

    tpl_hook.oops = printf;  /* errors to printf */

    tn = tpl_map("i#i#", i, TEST_LEN1, j, TEST_LEN2);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test68.tpl");
    tpl_free(tn);

    tn = tpl_map("i#i#", i2, TEST_LEN1, j2, TEST_LEN2);
    tpl_load(tn,TPL_FILE,"/tmp/test68.tpl");
    tpl_unpack(tn,0);
    tpl_free(tn);

    for(x=0;x<TEST_LEN1;x++) {
        if (i[x] != i2[x]) printf("mismatch (i)!\n");
    }
    for(x=0;x<TEST_LEN2;x++) {
        if (j[x] != j2[x]) printf("mismatch (j)!\n");
    }
    return(0);
}
