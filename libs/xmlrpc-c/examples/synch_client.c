/* A simple synchronous XML-RPC client program written in C. */

#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

#define NAME "XML-RPC C Test Client synch_client"
#define VERSION "1.0"

static void
die_if_fault_occurred(xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



int 
main(int           const argc, 
     const char ** const argv) {

    xmlrpc_env env;
    xmlrpc_value * resultP;
    const char * stateName;

    if (argc-1 > 0) {
        fprintf(stderr, "No arguments");
        exit(0);
    }

    /* Start up our XML-RPC client library. */
    xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);

    /* Initialize our error-handling environment. */
    xmlrpc_env_init(&env);

    /* Call the famous server at UserLand. */
    resultP = xmlrpc_client_call(&env, "http://betty.userland.com/RPC2",
                                 "examples.getStateName",
                                 "(i)", (xmlrpc_int32) 41);
    die_if_fault_occurred(&env);
    
    /* Get our state name and print it out. */
    xmlrpc_read_string(&env, resultP, &stateName);
    die_if_fault_occurred(&env);
    printf("%s\n", stateName);
    free((char*)stateName);
   
    /* Dispose of our result value. */
    xmlrpc_DECREF(resultP);

    /* Clean up our error-handling environment. */
    xmlrpc_env_clean(&env);
    
    /* Shutdown our XML-RPC client library. */
    xmlrpc_client_cleanup();

    return 0;
}
