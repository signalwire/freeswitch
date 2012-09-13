#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

#define TEST_LEN 10

int main() {
    tpl_node *tn;
    int i[TEST_LEN] = {1,2,3,4,5,6,7,8,9,10};

    tn = tpl_map("i#", i, TEST_LEN);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test65.tpl");
    tpl_free(tn);
    return(0);
}
