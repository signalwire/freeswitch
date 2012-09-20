#include <stdio.h>
#include "tpl.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
    tpl_node *tn;
    unsigned i;
    char *file = "/tmp/test23.tpl";
    int fd;

    tn = tpl_map("A(u)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FILE, file);
    tpl_free(tn);

    if ( (fd=open( file,O_RDONLY)) == -1) {
        printf("failed to open %s: %s", file, strerror(errno));
    }

    tn = tpl_map("A(u)",&i);
    tpl_load(tn, TPL_FD, fd);
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);
    tpl_free(tn);
    return(0);
}
