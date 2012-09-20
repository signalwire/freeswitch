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
    int x,y;

    tpl_hook.oops = printf;  /* errors to printf */

    tn = tpl_map("A(i#i#)", i, TEST_LEN1, j, TEST_LEN2);
    for(y=0; y < 2; y++) {
        for(x=0; x < TEST_LEN1; x++) i[x] += 1;
        for(x=0; x < TEST_LEN2; x++) j[x] -= 1;
        tpl_pack(tn,1);
    }
    tpl_dump(tn,TPL_FILE,"/tmp/test69.tpl");
    tpl_free(tn);

    tn = tpl_map("A(i#i#)", i2, TEST_LEN1, j2, TEST_LEN2);
    tpl_load(tn,TPL_FILE,"/tmp/test69.tpl");
    while (tpl_unpack(tn,1) > 0) {
        for(x=0; x < TEST_LEN1; x++) printf("%d ", i2[x]);
        printf("\n");
        for(x=0; x < TEST_LEN2; x++) printf("%d ", j2[x]);
        printf("\n");
    }
    tpl_free(tn);
    return(0);
}
