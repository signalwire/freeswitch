#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"

int main() {
    char *fmt, c;
    fmt = tpl_peek(TPL_FILE|TPL_DATAPEEK, "test99.tpl", "c", &c);
    if (fmt) {
        printf("%s\n",fmt);
        free(fmt);
        printf("%c\n", c);
    }
    return 0;
}
