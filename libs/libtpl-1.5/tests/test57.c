#include "tpl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define S1_LEN 6
#define S2_LEN 4

struct has_strings {
    char a;
    char s1[S1_LEN];
    char s2[S2_LEN];
};

int main(int argc,char*argv[]) {
    tpl_node *tn;
    struct has_strings hs;

    strncpy(hs.s1, "draco",S1_LEN);
    strncpy(hs.s2, "po",S2_LEN);

    tn = tpl_map("c#c#", hs.s1, S1_LEN, hs.s2, S2_LEN);
    tpl_pack(tn,0);
    tpl_dump(tn,TPL_FILE,"/tmp/test57.tpl");
    tpl_free(tn);

    return(0);
}
