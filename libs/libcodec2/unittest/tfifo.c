/* 
   tfifo.c
   David Rowe
   Nov 19 2012

   Takes FIFOs, in particular thread safety.
*/

#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include "fifo.h"

#define FIFO_SZ  1024
#define WRITE_SZ 10
#define READ_SZ  8  
#define N_MAX    100
#define LOOPS    1000000

int run_thread = 1;
struct FIFO *f;

void writer(void);
void *writer_thread(void *data);
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define USE_THREADS
//#define USE_MUTEX

int main() {
    pthread_t awriter_thread;
    int    i,j;
    short  read_buf[READ_SZ];
    int    n_out = 0;
    int    sucess;

    f = fifo_create(FIFO_SZ);
    #ifdef USE_THREADS
    pthread_create(&awriter_thread, NULL, writer_thread, NULL);
    #endif

    for(i=0; i<LOOPS; ) {
        #ifndef USE_THREADS
        writer();
        #endif

        #ifdef USE_MUTEX
        pthread_mutex_lock(&mutex);
        #endif
        sucess = (fifo_read(f, read_buf, READ_SZ) == 0);
        #ifdef USE_MUTEX
        pthread_mutex_unlock(&mutex);
        #endif

	if (sucess) {
	    for(j=0; j<READ_SZ; j++) {
                if (read_buf[j] != n_out) 
                    printf("error: %d %d\n", read_buf[j], n_out);
                n_out++;
                if (n_out == N_MAX)
                    n_out = 0;
            }
            i++;
        }
 
    }

    #ifdef USE_THREADS
    run_thread = 0;
    pthread_join(awriter_thread,NULL);
    #endif

    return 0;
}

int    n_in = 0;

void writer(void) {
    short  write_buf[WRITE_SZ];
    int    i;

    if ((FIFO_SZ - fifo_used(f)) > WRITE_SZ) {
        for(i=0; i<WRITE_SZ; i++) {
            write_buf[i] = n_in++;
            if (n_in == N_MAX)
                n_in = 0;
        }
        #ifdef USE_MUTEX
        pthread_mutex_lock(&mutex);
        #endif
        fifo_write(f, write_buf, WRITE_SZ);
        pthread_mutex_unlock(&mutex);
    }
}

void *writer_thread(void *data) {

    while(run_thread) {
        writer();
    }

    return NULL; 
}
