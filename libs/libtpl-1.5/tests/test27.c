#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
    tpl_node *tn;
    unsigned i,perms;
    char *file = "/tmp/test27.tpl";
    int fd;

    perms = S_IRUSR|S_IWUSR; 
    if ( (fd=open( file,O_RDWR|O_CREAT|O_TRUNC,perms)) == -1) {
        printf("failed to open %s: %s", file, strerror(errno));
        exit(-1);
    }

    tn = tpl_map("A(u)",&i);
    for(i=0;i<10;i++) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FD, fd);
    tpl_free(tn);

    lseek(fd,0,SEEK_SET);   /* re-position fd to start of file */

    tn = tpl_map("A(u)",&i);
    tpl_load(tn, TPL_FD, fd);
    while (tpl_unpack(tn,1) > 0) printf("i is %d\n", i);
    tpl_free(tn);
    return(0);
}
