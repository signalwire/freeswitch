#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "tpl.h"

struct str_holder {
    char str[10];
};


/* the real purpose of this test is to be run under dbx "check -all" 
 * mode to ensure the level-0 data are freed when replaced (i.e. the
 * level 0 nodes are packed more than once, this is an edge case but
 * the required behavior is to free the previously-packed data when
 * packing the new replacement data). Do the test for s,S,and B types.
 */
int main() {
    tpl_node *tn;
    char *s;
    struct str_holder sh;
    tpl_bin bin;

    /* test a replacement pack (s type) of the level 0 node */
    tn = tpl_map("s", &s);
    s = "alpha";
    tpl_pack(tn,0);  /* copies alpha */
    s = "beta"; 
    tpl_pack(tn,0);  /* should free alpha, copy beta */
    tpl_dump(tn,TPL_FILE,"/tmp/test64_0.tpl");
    tpl_free(tn);

    /* print out dumped tpl */
    s = "";
    tn = tpl_map("s", &s);
    tpl_load(tn,TPL_FILE,"/tmp/test64_0.tpl");
    tpl_unpack(tn,0);
    printf("s is %s\n", s);
    free(s);
    tpl_free(tn);

    /* test replacement pack (S type) of the level 0 node */
    tn = tpl_map("c#", sh.str, 10);
    strncpy(sh.str, "gamma", 10);
    tpl_pack(tn,0);  /* copies gamma */
    strncpy(sh.str, "delta", 10);
    tpl_pack(tn,0);  /* should free gamma, copy delta */
    tpl_dump(tn,TPL_FILE,"/tmp/test64_1.tpl");
    tpl_free(tn);

    /* print out dumped tpl */
    sh.str[0] = '\0';
    tn = tpl_map("c#", sh.str, 10);
    tpl_load(tn,TPL_FILE,"/tmp/test64_1.tpl");
    tpl_unpack(tn,0);
    printf("sh.str is %s\n", sh.str);
    tpl_free(tn);

    /* test replacement pack (B type) of the level 0 node */
    tn = tpl_map("B", &bin);
    bin.addr = "epsilon";
    bin.sz = strlen("epsilon")+1;
    tpl_pack(tn,0);  /* copies epsilon */
    bin.addr = "zeta";
    bin.sz = strlen("zeta")+1;
    tpl_pack(tn,0);  /* should free epsilon, copy zeta */
    tpl_dump(tn,TPL_FILE,"/tmp/test64_2.tpl");
    tpl_free(tn);

    /* print out dumped tpl */
    bin.addr = "";
    bin.sz = 1;
    tn = tpl_map("B", &bin);
    tpl_load(tn,TPL_FILE,"/tmp/test64_2.tpl");
    tpl_unpack(tn,0);
    printf("bin.addr is %s, size %d\n", (char*)(bin.addr), bin.sz);
    free(bin.addr);
    tpl_free(tn);
    return(0);

}
