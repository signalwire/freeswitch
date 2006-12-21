/* A simple standalone XML-RPC server based on Abyss that contains a
   simple one-thread request processing loop.  

   xmlrpc_sample_add_server.c is a server that does the same thing, but
   does it by running a full Abyss daemon in the background, so it has
   less control over how the requests are served.
*/

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#include "config.h"  /* information about this build environment */


static void
setupSignalHandlers(void) {

    /* In UNIX, when you try to write to a socket that has been closed
       from the other end, your write fails, but you also get a SIGPIPE
       signal.  That signal will kill you before you even have a chance
       to see the write fail unless you catch, block, or ignore it.
       If a client should connect to us and then disconnect before we've
       sent our response, we see this socket-closed behavior.  We
       obviously don't want to die just because a client didn't complete
       an RPC, so we ignore SIGPIPE.
    */
    struct sigaction mysigaction;
    
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, NULL);
}



static xmlrpc_value *
sample_add(xmlrpc_env *   const envP, 
           xmlrpc_value * const paramArrayP,
           void *         const userData ATTR_UNUSED) {
    
    xmlrpc_int x, y, z;

    /* Parse our argument array. */
    xmlrpc_decompose_value(envP, paramArrayP, "(ii)", &x, &y);
    if (envP->fault_occurred)
        return NULL;

    /* Add our two numbers. */
    z = x + y;

    /* Return our result. */
    return xmlrpc_build_value(envP, "i", z);
}



int 
main(int           const argc, 
     const char ** const argv) {

    TServer abyssServer;
    xmlrpc_registry * registryP;
    xmlrpc_env env;

    if (argc-1 != 1) {
        fprintf(stderr, "You must specify 1 argument:  The TCP port number "
                "on which to listen for XML-RPC calls.  "
                "You specified %d.\n",  argc-1);
        exit(1);
    }
    
    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method(
        &env, registryP, NULL, "sample.add", &sample_add, NULL);

    MIMETypeInit();

    ServerCreate(&abyssServer, "XmlRpcServer", atoi(argv[1]), 
                 NULL, NULL);
    
    xmlrpc_server_abyss_set_handlers(&abyssServer, registryP);

    ServerInit(&abyssServer);

    setupSignalHandlers();

    while (1) {
        printf("Waiting for next RPC...\n");

        ServerRunOnce2(&abyssServer, ABYSS_FOREGROUND);
            /* This waits for the next connection, accepts it, reads the
               HTTP POST request, executes the indicated RPC, and closes
               the connection.
            */
    }

    return 0;
}
