#include <stdio.h>
#include <stdlib.h>
#include "tpl.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
    tpl_node *tn;
    unsigned i, sum=0;
    int fd[2], pid,rc;
    void *img;
    size_t sz;

    pipe(fd);
    if ( (pid = fork()) == 0) {   /* child */

        rc = tpl_gather(TPL_GATHER_BLOCKING,fd[0],&img,&sz);
        if (rc != 1) {
            printf("error: rc non-zero: %d\n", rc);
            exit(-1);
        }
        tn = tpl_map("A(u)",&i);
        tpl_load(tn, TPL_MEM, img,sz);
        while (tpl_unpack(tn,1) > 0) sum += i;
        tpl_free(tn);
        printf("sum is %d\n", sum);

    } else if (pid > 0) {         /* parent */

        tn = tpl_map("A(u)",&i);
        for(i=0;i<10000;i++) tpl_pack(tn,1);
        tpl_dump(tn,TPL_FD, fd[1] );
        tpl_free(tn);

        close(fd[1]);
        waitpid(pid,NULL,0);

    } else if (pid == -1) {
        perror("fork error");
    }
    return(0);
}
