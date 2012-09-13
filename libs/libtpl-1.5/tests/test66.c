#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

#define TEST_LEN1 10
#define TEST_LEN2 5

int main() {
    tpl_node *tn;
    int i[TEST_LEN1] = {1,2,3,4,5,6,7,8,9,10};
    int j[TEST_LEN2] = {5,4,3,2,1};

    tn = tpl_map("i#i#", i, TEST_LEN1, j, TEST_LEN2);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test66.tpl");
    tpl_free(tn);
    return(0);
}
