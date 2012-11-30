/* Same as xmlrpc_sample_add_client.c, except the call is interruptible,
   both by timeout and by control-C.
*/

#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

static int interrupt;
    /* This is a flag telling libxmlrpc_client to abort whatever it's
       doing.  It's global because we set it with a signal handler.
    */

static void 
die_if_fault_occurred (xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



static void 
interruptRpc(int const signalClass) {

    switch (signalClass) {
    case SIGINT:
        printf("SIGINT signal received.\n");
        break;
    case SIGALRM:
        printf("SIGALRM signal received.\n");
        break;
    default:
        printf("Internal error: signal of class %u caught even though "
               "we didn't set up a handler for that class\n", signalClass);
    };

    interrupt = 1;
}



static void
setupSignalHandlers(void) {

    struct sigaction mysigaction;
    
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;

    /* Usually, this signal indicates the user pressed Ctl-C */
    mysigaction.sa_handler = interruptRpc;
    sigaction(SIGINT, &mysigaction, NULL);
    /* This signal indicates a timed alarm you requested happened */
    sigaction(SIGALRM, &mysigaction, NULL);
}



static void
addInterruptibly(xmlrpc_client * const clientP,
                 const char *    const serverUrl,
                 int             const addend,
                 int             const adder) {

    const char * const methodName = "sample.add";

    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_int32 sum;

    xmlrpc_env_init(&env);

    printf("Making XMLRPC call to server url '%s' method '%s' "
           "to request the sum "
           "of %d and %d...\n", serverUrl, methodName, addend, adder);

    interrupt = 0;  /* Global variable */

    alarm(2); /* Interrupt the call if it hasn't finished 2 seconds from now */

    /* Make the remote procedure call */

    xmlrpc_client_call2f(&env, clientP, serverUrl, methodName, &resultP,
                         "(ii)", (xmlrpc_int32) addend, (xmlrpc_int32) adder);
    die_if_fault_occurred(&env);

    alarm(0);  /* Cancel alarm, if it hasn't happened yet */
    
    /* Get our sum and print it out. */
    xmlrpc_read_int(&env, resultP, &sum);
    die_if_fault_occurred(&env);
    printf("The sum is %d\n", sum);
    
    /* Dispose of our result value. */
    xmlrpc_DECREF(resultP);

    xmlrpc_env_clean(&env);
}



int 
main(int           const argc, 
     const char ** const argv) {

    const char * const serverUrl = "http://localhost:8080/RPC2";

    xmlrpc_env env;
    struct xmlrpc_clientparms clientParms;
    xmlrpc_client * clientP;

    if (argc-1 > 0) {
        fprintf(stderr, "This program has no arguments\n");
        exit(1);
    }

    setupSignalHandlers();

    /* Initialize our error-handling environment. */
    xmlrpc_env_init(&env);

    /* Required before any use of Xmlrpc-c client library: */
    xmlrpc_client_setup_global_const(&env);
    die_if_fault_occurred(&env);

    clientParms.transport = "curl";

    /* Create a client object */
    xmlrpc_client_create(&env, 0, NULL, NULL,
                         &clientParms, XMLRPC_CPSIZE(transport),
                         &clientP);

    die_if_fault_occurred(&env);

    xmlrpc_client_set_interrupt(clientP, &interrupt);

    /* If our server is running 'xmlrpc_sample_add_server' normally, the
       RPC will finish almost instantly.  UNLESS the adder is 1, in which
       case said server is programmed to take 3 seconds to do the
       computation, thus allowing us to demonstrate a timeout or CTL-C.
    */

    addInterruptibly(clientP, serverUrl, 5, 7);
        /* Should finish instantly */

    addInterruptibly(clientP, serverUrl, 5, 1);
        /* Should time out after 2 seconds */

    xmlrpc_env_clean(&env);
    xmlrpc_client_destroy(clientP);
    xmlrpc_client_teardown_global_const();

    return 0;
}

