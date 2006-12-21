/* A simple asynchronous XML-RPC client written in C, as an example of
   Xmlrpc-c asynchronous RPC facilities.  This is the same as the 
   simpler synchronous client xmlprc_sample_add_client.c, except that
   it adds 3 different pairs of numbers with the summation RPCs going on
   simultaneously.
*/

#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

#define NAME "Xmlrpc-c Asynchronous Test Client"
#define VERSION "1.0"

static void 
die_if_fault_occurred (xmlrpc_env *env) {
    if (env->fault_occurred) {
        fprintf(stderr, "Something failed. %s (XML-RPC fault code %d)\n",
                env->fault_string, env->fault_code);
        exit(1);
    }
}



static void 
handle_sample_add_response(const char *   const server_url,
                           const char *   const method_name,
                           xmlrpc_value * const param_array,
                           void *         const user_data ATTR_UNUSED,
                           xmlrpc_env *   const faultP,
                           xmlrpc_value * const resultP) {
    
    xmlrpc_env env;
    xmlrpc_int addend, adder;
    
    /* Initialize our error environment variable */
    xmlrpc_env_init(&env);

    /* Our first four arguments provide helpful context.  Let's grab the
       addends from our parameter array. 
    */
    xmlrpc_decompose_value(&env, param_array, "(ii)", &addend, &adder);
    die_if_fault_occurred(&env);

    printf("RPC with method '%s' at URL '%s' to add %d and %d "
           "has completed\n", method_name, server_url, addend, adder);
    
    if (faultP->fault_occurred)
        printf("The RPC failed.  %s", faultP->fault_string);
    else {
        xmlrpc_int sum;

        xmlrpc_read_int(&env, resultP, &sum);
        die_if_fault_occurred(&env);

        printf("The sum is  %d\n", sum);
    }
}



int 
main(int           const argc, 
     const char ** const argv ATTR_UNUSED) {

    char * const url = "http://localhost:8080/RPC2";
    char * const methodName = "sample.add";

    xmlrpc_env env;
    xmlrpc_int adder;

    if (argc-1 > 0) {
        fprintf(stderr, "This program has no arguments\n");
        exit(1);
    }

    /* Initialize our error environment variable */
    xmlrpc_env_init(&env);

    /* Create the Xmlrpc-c client object */
    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
    die_if_fault_occurred(&env);

    for (adder = 0; adder < 3; ++adder) {
        printf("Making XMLRPC call to server url '%s' method '%s' "
               "to request the sum "
               "of 5 and %d...\n", url, methodName, adder);

        /* request the remote procedure call */
        xmlrpc_client_call_asynch(url, methodName,
                                  handle_sample_add_response, NULL,
                                  "(ii)", (xmlrpc_int32) 5, adder);
        die_if_fault_occurred(&env);
    }
    
    printf("RPCs all requested.  Waiting for & handling responses...\n");

    /* The following is what calls handle_sample_add_response() (3 times) */
    xmlrpc_client_event_loop_finish_asynch();

    printf("All RPCs finished.\n");

    /* Destroy the Xmlrpc-c client object */
    xmlrpc_client_cleanup();

    return 0;
}
