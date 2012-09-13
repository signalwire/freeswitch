#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#include <errno.h>
#include "tpl.h"

#define DEBUG 0
#define FILE_BUFLEN 500

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
    if (num_tpls == 3) return -1;
    return 0;
    
}

int main(int argc, char *argv[]) {
    char *files[] = {"test54_0.tpl", "test54_1.tpl", "test54_2.tpl", "test54_3.tpl","test54_4.tpl", NULL};
    char **f;
    char buf[FILE_BUFLEN];
    int rc,fd;
    tpl_gather_t *gs=NULL;

    for (f = files; *f; f++) {
        if (DEBUG) printf("file is %s\n", *f);
        if ( ( fd = open(*f, O_RDONLY) ) == -1) {
            printf("error - can't open %s: %s\n", *f, strerror(errno));
            exit(-1);
        }
        rc = read(fd,&buf,FILE_BUFLEN);  /* read whole file (no points for style) */
        if (rc == -1) {
            printf("error - can't read %s: %s\n", *f, strerror(errno));
            exit(-1);
        }
        if (tpl_gather(TPL_GATHER_MEM,buf, rc, &gs, tpl_cb, NULL) <= 0) {
            printf("tpl_gather_mem returned <= 0, exiting\n");
            exit(-1);
        }
        close(fd);
    }
    printf("num_tpls: %d, sum: %d\n", num_tpls, sum_tpls);
    return(0);
}
