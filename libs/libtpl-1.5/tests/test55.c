#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include "tpl.h"

#define DEBUG 0

extern tpl_hook_t tpl_hook;
int num_tpls = 0, sum_tpls = 0;

int tpl_cb(void *tpl, size_t tpllen, void*data) {
    int i;
    tpl_node *tn;

    tpl_hook.oops = printf;

    if (DEBUG) printf("obtained tpl of length %d\n", (int)tpllen);
    tn = tpl_map("A(i)", &i);
    tpl_load(tn, TPL_MEM, tpl, tpllen);
    num_tpls++;
    while (tpl_unpack(tn,1) > 0) sum_tpls += i;
    tpl_free(tn); 
    /* this next line is a hack to test the callback's ability
     * to abort further tpl processing by returning < 0 */
    if (num_tpls == 1) return -1;
    return 0;
    
}

int main() {
    FILE *f1,*f2;
    int fdflags,fd,fd1,fd2;
    int selrc, maxfd;
    tpl_gather_t *gs1=NULL,*gs2=NULL,**gs;
    struct timeval tv;
    fd_set rset;


    f1 = popen("cat test26_0.tpl;sleep 1; cat test26_1.tpl", "r");
    fd1 = fileno(f1); 
    fdflags = fcntl(fd1, F_GETFL, 0);
    fcntl( fd1, F_SETFL, fdflags | O_NONBLOCK);

    f2 = popen("cat test26_2.tpl;sleep 1; cat test26_3.tpl", "r");
    fd2 = fileno(f2); 
    fdflags = fcntl(fd2, F_GETFL, 0);
    fcntl( fd2, F_SETFL, fdflags | O_NONBLOCK);

    while (1) {
        FD_ZERO( &rset );
        if (fd1 >= 0) FD_SET( fd1, &rset );
        if (fd2 >= 0) FD_SET( fd2, &rset );

        if (fd1 == -1 && fd2 == -1) {
            printf("%d tpls gathered.\n",num_tpls);
            printf("%d is their sum.\n",sum_tpls);
            return(0);
        }

        maxfd=0;
        if (fd1>maxfd) maxfd = fd1;
        if (fd2>maxfd) maxfd = fd2;

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        selrc = select(maxfd+1, &rset, NULL, NULL, &tv );
        if (selrc == -1) {
           perror("select()");
        } else if (selrc) {
            for(fd=0;fd<maxfd+1;fd++) {
                if ( FD_ISSET(fd, &rset) ) {
                    if (DEBUG) printf("fd %d readable\n", fd);
                    gs = (fd1 == fd) ? &gs1 : &gs2;
                    if (tpl_gather(TPL_GATHER_NONBLOCKING,fd,gs,tpl_cb,NULL) <= 0) {
                        if (fd1 == fd) {pclose(f1); fd1 = -1; }
                        if (fd2 == fd) {pclose(f2); fd2 = -1; }
                    } else {
                        if (DEBUG) printf("tpl_gather >0\n");
                    }
                }
            }
        } else {
            if (DEBUG) printf("timeout\n");  
        }
    }
    return(0);
}
