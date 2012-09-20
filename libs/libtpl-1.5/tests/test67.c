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

    tpl_hook.oops = printf;  /* errors to printf */

    tn = tpl_map("i#i#", i, TEST_LEN1, j, TEST_LEN2);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test67.tpl");
    tpl_free(tn);

    /* Expect success on the next line */
    tn = tpl_map("i#i#", i, TEST_LEN1, j, TEST_LEN2);
    if (tpl_load(tn,TPL_FILE,"/tmp/test67.tpl") < 0) {
        printf("load failed\n");
    } else {
        printf("load succeeded\n");
    }
    tpl_free(tn);

    /* Expect failure on the next line (TEST_LEN2 != TEST_LEN2-1) */
    tn = tpl_map("i#i#", i, TEST_LEN1, j, (TEST_LEN2 - 1));
    if (tpl_load(tn,TPL_FILE,"/tmp/test67.tpl") < 0) {
        printf("load failed\n");
    } else {
        printf("load succeeded\n");
    }
    tpl_free(tn);
    return(0);
}
