/* A simple standalone XML-RPC server program based on Abyss that processes a
   single RPC from an existing TCP connection on Standard Input.

   A typical example of where this would be useful is with an Inetd
   "super server."

   xmlrpc_sample_add_server.c is a server that does the same thing,
   but you give it a TCP port number and it listens for TCP connecitons
   and processes RPCs ad infinitum.  xmlrpc_socket_server.c is halfway
   in between those -- you give it an already bound and listening
   socket, and it lists for TCP connections and processes RPCs ad
   infinitum.

   Here is an easy way to test this program:

     socketexec --accept --local_port=8080 --stdin -- ./xmlrpc_inetd_server

   Now run the client program 'xmlrpc_sample_add_client'.  Socketexec
   will accept the connection that the client program requests and pass it
   to this program on Standard Input.  This program will perform the RPC,
   respond to the client, then exit.
*/

#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#ifndef WIN32
#include <unistd.h>
#endif

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
#ifndef WIN32
    struct sigaction mysigaction;
    
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, NULL);
#endif
}



static xmlrpc_value *
sample_add(xmlrpc_env *   const envP, 
           xmlrpc_value * const paramArrayP,
           void *         const serverInfo,
           void *         const channelInfo) {
    
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

    struct xmlrpc_method_info3 const methodInfo = {
        .methodName     = "sample.add",
        .methodFunction = &sample_add,
        .serverInfo = NULL
    };
    TServer abyssServer;
    xmlrpc_registry * registryP;
    xmlrpc_env env;

    if (argc-1 != 0) {
        fprintf(stderr, "There are no arguments.  You must supply a "
                "bound socket on which to listen for client connections "
                "as Standard Input\n");
        if (argv) {} /* silence unused parameter warning */
        exit(1);
    }
    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method3(&env, registryP, &methodInfo);

    ServerCreateNoAccept(&abyssServer, "XmlRpcServer", NULL, NULL);
    
    xmlrpc_server_abyss_set_handlers(&abyssServer, registryP);

    setupSignalHandlers();

    ServerRunConn(&abyssServer, STDIN_FILENO);
        /* This reads the HTTP POST request from Standard Input and
           executes the indicated RPC.
        */

    ServerFree(&abyssServer);

    return 0;
}
