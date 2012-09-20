#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    char *fmt;
    fmt = tpl_peek(TPL_FILE, "test85.tpl");
    if (fmt) {
        printf("%s\n",fmt);
        free(fmt);
    }
    return 0;
}
