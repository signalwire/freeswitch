    #include "tpl.h"

    int main() {
        tpl_node *tn;
        char *s;

        tn = tpl_map( "A(s)", &s );

        s = "bob";
        tpl_pack(tn, 1);

        s = "betty";
        tpl_pack(tn, 1);

        tpl_dump(tn, TPL_FILE, "/tmp/test49.tpl");
        tpl_free(tn);
        return(0);
    }
