#include <stdio.h>
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
    int fd[2], pid;
    char c;

    pipe(fd);
    if ( (pid = fork()) == 0) {   /* child */

        tn = tpl_map("A(u)",&i);
        tpl_load(tn, TPL_FD, fd[0]);
        while (tpl_unpack(tn,1) > 0) sum += i;
        tpl_free(tn);
        printf("sum is %d\n", sum);

        tn = tpl_map("A(c)",&c);
        tpl_load(tn, TPL_FD, fd[0]);
        while (tpl_unpack(tn,1) > 0) printf("%c",c);
        tpl_free(tn);
        printf("\n");

    } else if (pid > 0) {         /* parent */

        tn = tpl_map("A(u)",&i);
        for(i=0;i<10000;i++) tpl_pack(tn,1);
        tpl_dump(tn,TPL_FD, fd[1] );
        tpl_free(tn);

        tn = tpl_map("A(c)",&c);
        for(c='a';c<='z';c++) tpl_pack(tn,1);
        tpl_dump(tn,TPL_FD, fd[1] );
        tpl_free(tn);

        waitpid(pid,NULL,0);

    } else if (pid == -1) {
        perror("fork error");
    }
    return(0);
}
