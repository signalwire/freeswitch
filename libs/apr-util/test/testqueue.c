/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <apr_thread_proc.h>
#include <apr_errno.h>
#include <apr_general.h>
#include <apr_getopt.h>
#include <apr_strings.h>
#include "errno.h"
#include <stdio.h>
#include <stdlib.h>
#include <apr_time.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <apr_portable.h>
#include "apr_queue.h"

#if !APR_HAS_THREADS
int main(void)
{
    fprintf(stderr,
            "This program won't work on this platform because there is no "
            "support for threads.\n");
    return 0;
}
#else /* !APR_HAS_THREADS */

apr_pool_t *context;
int consumer_activity=400;
int producer_activity=300;
int verbose=0;
static void * APR_THREAD_FUNC consumer(apr_thread_t *thd, void *data);
static void * APR_THREAD_FUNC producer(apr_thread_t *thd, void *data);
static void usage(void);

static void * APR_THREAD_FUNC consumer(apr_thread_t *thd, void *data)
{
    long sleeprate;
    apr_queue_t *q = (apr_queue_t*)data;
    apr_status_t rv;
    int val;
    void *v;
    char current_thread_str[30];
    apr_os_thread_t current_thread = apr_os_thread_current();

    apr_snprintf(current_thread_str, sizeof current_thread_str,
                 "%pT", &current_thread);

    sleeprate = 1000000/consumer_activity;
    apr_sleep( (rand() % 4 ) * 1000000 ); /* sleep random seconds */
    while (1) {
        do {
            rv = apr_queue_pop(q, &v);
            if (rv == APR_EINTR) {
                fprintf(stderr, "%s\tconsumer intr\n", current_thread_str);
            }

        } while (rv == APR_EINTR) ;
        if (rv != APR_SUCCESS) {
            if (rv == APR_EOF) {
                fprintf(stderr, "%s\tconsumer:queue terminated APR_EOF\n", current_thread_str);
                rv=APR_SUCCESS;
            }
            else 
                fprintf(stderr, "%s\tconsumer thread exit rv %d\n", current_thread_str, rv);
            apr_thread_exit(thd, rv);
            return NULL;
        }
        val = *(int*)v;
        if (verbose)
            fprintf(stderr,  "%s\tpop %d\n", current_thread_str, val);
        apr_sleep( sleeprate ); /* sleep this long to acheive our rate */
    }
    /* not reached */
    return NULL;
} 

static void * APR_THREAD_FUNC producer(apr_thread_t *thd, void *data)
{
    int i=0;
    long sleeprate;
    apr_queue_t *q = (apr_queue_t*)data;
    apr_status_t rv;
    int *val;
    char current_thread_str[30];
    apr_os_thread_t current_thread = apr_os_thread_current();

    apr_snprintf(current_thread_str, sizeof current_thread_str,
                 "%pT", &current_thread);

    sleeprate = 1000000/producer_activity;
    apr_sleep( (rand() % 4 ) * 1000000 ); /* sleep random seconds */
        
    while(1) {
        val = apr_palloc(context, sizeof(int));
        *val=i;
        if (verbose)
            fprintf(stderr,  "%s\tpush %d\n", current_thread_str, *val);
        do {
            rv = apr_queue_push(q, val);
            if (rv == APR_EINTR) 
                fprintf(stderr, "%s\tproducer intr\n", current_thread_str);
        } while (rv == APR_EINTR);

        if (rv != APR_SUCCESS) {
            if (rv == APR_EOF) {
                fprintf(stderr, "%s\tproducer: queue terminated APR_EOF\n", current_thread_str);
                rv = APR_SUCCESS;
            }
            else
                fprintf(stderr, "%s\tproducer thread exit rv %d\n", current_thread_str, rv);
            apr_thread_exit(thd, rv);
            return NULL;
        }
        i++;
        apr_sleep( sleeprate ); /* sleep this long to acheive our rate */
    }
   /* not reached */
    return NULL;
} 

static void usage(void)
{
    fprintf(stderr,"usage: testqueue -p n -P n -c n -C n -q n -s n\n");
    fprintf(stderr,"-c # of consumer\n");
    fprintf(stderr,"-C amount they consumer before dying\n");
    fprintf(stderr,"-p # of producers\n");
    fprintf(stderr,"-P amount they produce before dying\n");
    fprintf(stderr,"-q queue size\n");
    fprintf(stderr,"-s amount of time to sleep before killing it\n");
    fprintf(stderr,"-v verbose\n");
}

int main(int argc, const char* const argv[])
{
    apr_thread_t **t;
    apr_queue_t *queue;
    int i;
    apr_status_t rv;
    apr_getopt_t *opt;
    const char *optarg;
    char c;
    int numconsumers=3;
    int numproducers=4;
    int queuesize=100;
    int sleeptime=30;
    char errorbuf[200];

    apr_initialize();
    srand((unsigned int)apr_time_now());
    printf("APR Queue Test\n======================\n\n");
    
    printf("%-60s", "Initializing the context"); 
    if (apr_pool_create(&context, NULL) != APR_SUCCESS) {
        fflush(stdout);
        fprintf(stderr, "Failed.\nCould not initialize\n");
        exit(-1);
    }
    printf("OK\n");

    apr_getopt_init(&opt, context, argc, argv);
    while ((rv = apr_getopt(opt, "p:c:P:C:q:s:v", &c, &optarg))
            == APR_SUCCESS) {
        switch (c)  {
        case 'c':
            numconsumers = atoi( optarg);
            break;
        case 'p':
            numproducers = atoi( optarg);
            break;
        case 'C':
            consumer_activity = atoi( optarg);
            break;
        case 'P':
            producer_activity = atoi( optarg);
            break;
        case 's':
            sleeptime= atoi(optarg);
            break;
        case 'q':
            queuesize = atoi(optarg);
            break;
        case 'v':
            verbose= 1;
            break;
        default:
            usage();
            exit(-1);
        }
    }
    /* bad cmdline option?  then we die */
    if (rv != APR_EOF || opt->ind < opt->argc) {
        usage();
        exit(-1);
    }



    printf("test stats %d consumers (rate %d/sec) %d producers (rate %d/sec) queue size %d sleep %d\n",
            numconsumers,consumer_activity, numproducers, producer_activity, queuesize,sleeptime); 
    printf("%-60s", "Initializing the queue"); 
    rv  = apr_queue_create(&queue, queuesize, context);

    if (rv != APR_SUCCESS) {
        fflush(stdout);
        fprintf(stderr, "Failed\nCould not create queue %d\n",rv);
        apr_strerror(rv, errorbuf,200);
        fprintf(stderr,"%s\n",errorbuf);
        exit(-1);
    }
    printf("OK\n");

    t = apr_palloc( context, sizeof(apr_thread_t*) * (numconsumers+numproducers));
    printf("%-60s", "Starting consumers"); 
    for (i=0;i<numconsumers;i++) {
        rv = apr_thread_create(&t[i], NULL, consumer, queue, context);
        if (rv != APR_SUCCESS) {
            apr_strerror(rv, errorbuf,200);
            fprintf(stderr, "Failed\nError starting consumer thread (%d) rv=%d:%s\n",i, rv,errorbuf);
            exit(-1);

        }
    }
    for (i=numconsumers;i<(numconsumers+numproducers);i++) {
        rv = apr_thread_create(&t[i], NULL, producer, queue, context);
        if (rv != APR_SUCCESS) {
            apr_strerror(rv, errorbuf,200);
            fprintf(stderr, "Failed\nError starting producer thread (%d) rv=%d:%s\n",i, rv,errorbuf);
            exit(-1);

        }
    }

    printf("OK\n");
    printf("%-60s", "Sleeping\n"); 
    apr_sleep( sleeptime * 1000000 ); /* sleep 10 seconds */
    printf("OK\n");

    printf("%-60s", "Terminating queue"); 
    rv = apr_queue_term(queue);
    if (rv != APR_SUCCESS) {
        apr_strerror(rv, errorbuf,200);
        fprintf( stderr, "apr_queue_term failed  %d:%s\n",rv,errorbuf);
    }
    printf("OK\n");


    printf("%-60s", "Waiting for threads to exit\n");
    fflush(stdout);
    for (i=0;i<numconsumers+numproducers;i++) {
        apr_thread_join(&rv, t[i]);
        if (rv != 0 ) {
            apr_strerror(rv, errorbuf,200);
            if (i<numconsumers) 
                fprintf( stderr, "consumer thread %d failed rv %d:%s\n",i,rv,errorbuf);
            else
                fprintf( stderr, "producer thread %d failed rv %d:%s\n",i,rv,errorbuf);
        }
    }

    printf("OK\n");

    apr_terminate();

    return 0;
}

#endif /* !APR_HAS_THREADS */
