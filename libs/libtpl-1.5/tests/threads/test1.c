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
#include <pthread.h>

int fd[2];

void *thread_routine( void *arg ) {
    tpl_node *tn;
    int i,sum=0;

    /* child */
    tn = tpl_map("A(u)",&i);
    tpl_load(tn, TPL_FD, fd[0]);
    while (tpl_unpack(tn,1) > 0) sum += i;
    tpl_free(tn);
    printf("sum is %d\n", sum);
    return NULL;
}

int main() {
    tpl_node *tn;
    unsigned i;
    int status;
    pthread_t thread_id;
    void *thread_result;

    pipe(fd);
    if ( status = pthread_create( &thread_id, NULL, thread_routine, NULL )) {
        printf("failure: status %d\n", status);
        exit(-1);
    }
    /* parent */
    tn = tpl_map("A(u)",&i);
    for(i=0;i<10000;i++) tpl_pack(tn,1);
    tpl_dump(tn,TPL_FD, fd[1] );
    tpl_free(tn);

    status = pthread_join( thread_id, &thread_result );
    printf("thread result: %d %s\n", status, thread_result ? "non-null":"null");
}
