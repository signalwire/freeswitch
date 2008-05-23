/* A demonstration of using HTTP basic authentication with XML-RPC.
**
** In general, you shouldn't write XML-RPC servers which require basic
** authenticaion. (Few XML-RPC clients are capable of it, and it's not part of
** any standard.) Instead, you should pass any authentication information
** as a regular XML-RPC parameter (or look into using SSL).
**
** But certain XML-RPC servers, including Zope, rely heavily on HTTP
** basic authentication. Here's how to talk to them. */

#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

#define NAME       "XML-RPC C Auth Client"
#define VERSION    "1.0"
#define SERVER_URL "http://localhost:8080/RPC2"



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
     const char ** const argv ATTR_UNUSED) {

    xmlrpc_env env;
    xmlrpc_server_info * serverP;
    xmlrpc_value * resultP;
    xmlrpc_int sum;
    
    if (argc-1 > 0) {
        fprintf(stderr, "There are no arguments.  You specified %d", argc-1);
        exit(1);
    }

    /* Start up our XML-RPC client library. */
    xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);
    xmlrpc_env_init(&env);

    /* Make a new object to represent our XML-RPC server. */
    serverP = xmlrpc_server_info_new(&env, SERVER_URL);
    die_if_fault_occurred(&env);

    /* Set up our authentication information. */
    xmlrpc_server_info_set_basic_auth(&env, serverP, "jrandom", "secret");
    die_if_fault_occurred(&env);

    resultP = 
        xmlrpc_client_call_server(
            &env, serverP, "sample.add", "(ii)", 
            (xmlrpc_int32) 5, (xmlrpc_int32) 7);
    die_if_fault_occurred(&env);

    /* Dispose of our server object. */
    xmlrpc_server_info_free(serverP);
    
    /* Get the authentication information and print it out. */
    xmlrpc_read_int(&env, resultP, &sum);
    die_if_fault_occurred(&env);
    printf("The sum is %d\n", sum);
    
    /* Dispose of our result value. */
    xmlrpc_DECREF(resultP);

    /* Shut down our XML-RPC client library. */
    xmlrpc_env_clean(&env);
    xmlrpc_client_cleanup();

    return 0;
}
