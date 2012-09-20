#include "tpl.h"

int main() {
    int i;
    char c;
    tpl_node *tn;

    tn = tpl_map("A(i)c", &i, &c);

    /* pack index number 0 (char c) */
    c = 'a';
    tpl_pack(tn, 0);  
   
    /* pack A(i) (that is, index number 1) a few times */
    i = 3;
    tpl_pack(tn, 1);
    i = 4;
    tpl_pack(tn, 1);

    tpl_dump(tn, TPL_FILE, "/tmp/test78.tpl");
    tpl_free(tn);
    return(0);
}
