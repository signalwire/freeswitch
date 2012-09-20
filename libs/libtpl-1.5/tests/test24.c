#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
    tpl_node *tn;
    unsigned i;
    char *file = "test24.tpl";
    int fd;

    if ( (fd=open( file,O_RDONLY)) == -1) {
        printf("failed to open %s: %s", file, strerror(errno));
        exit(-1);
    }

    tn = tpl_map("A(u)",&i);
    tpl_load(tn, TPL_FD, fd);
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);
    tpl_free(tn);
    return(0);
}
