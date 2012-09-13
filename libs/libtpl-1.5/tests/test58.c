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

    tn = tpl_map("c#c#", hs.s1, S1_LEN, hs.s2, S2_LEN);
    tpl_load(tn,TPL_FILE,"/tmp/test57.tpl");
    tpl_unpack(tn,0);
    tpl_free(tn);

    printf("hs.s1 length: %d\n", (int)strlen(hs.s1));
    printf("hs.s1: %s\n", hs.s1);
    printf("hs.s2 length: %d\n", (int)strlen(hs.s2));
    printf("hs.s2: %s\n", hs.s2);

    return(0);
}
