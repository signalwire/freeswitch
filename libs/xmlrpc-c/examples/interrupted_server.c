/* A simple standalone XML-RPC server based on Abyss.

   You can terminate this server in controlled fashion with a SIGTERM
   signal.

   xmlrpc_sample_add_server.c is a server that does the same thing with
   simpler code, but it is not interruptible with SIGTERM.
*/

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/abyss.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#include "config.h"  /* information about this build environment */


static void 
dieIfFailed(const char * const description,
            xmlrpc_env   const env) {

    if (env.fault_occurred) {
        fprintf(stderr, "%s failed. %s\n", description, env.fault_string);
        exit(1);
    }
}



static xmlrpc_server_abyss_t * serverToTerminateP;

static void 
sigtermHandler(int const signalClass ATTR_UNUSED) {
    
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_server_abyss_terminate(&env, serverToTerminateP);

    dieIfFailed("xmlrpc_server_abyss_terminate", env);
    
    xmlrpc_env_clean(&env);
}



static void
setupSigtermHandler(xmlrpc_server_abyss_t * const serverP) {

    struct sigaction mysigaction;

    serverToTerminateP = serverP;
    
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;
    mysigaction.sa_handler = sigtermHandler;
    sigaction(SIGTERM, &mysigaction, NULL);
}



static void
restoreSigtermHandler(void){

    struct sigaction mysigaction;

    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;
    mysigaction.sa_handler = SIG_DFL;
    sigaction(SIGTERM, &mysigaction, NULL);
}



static xmlrpc_value *
sample_add(xmlrpc_env *   const envP, 
           xmlrpc_value * const paramArrayP,
           void *         const serverInfo ATTR_UNUSED,
           void *         const channelInfo ATTR_UNUSED) {
    
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

    xmlrpc_server_abyss_parms serverparm;
    xmlrpc_server_abyss_t * serverP;
    xmlrpc_registry * registryP;
    xmlrpc_env env;
    xmlrpc_server_abyss_sig * oldHandlersP;

    if (argc-1 != 1) {
        fprintf(stderr, "You must specify 1 argument:  The TCP port number "
                "on which to listen for XML-RPC calls.  "
                "You specified %d.\n",  argc-1);
        exit(1);
    }

    xmlrpc_env_init(&env);

    xmlrpc_server_abyss_global_init(&env);
    dieIfFailed("xmlrpc_server_abyss_global_init", env);
    
    registryP = xmlrpc_registry_new(&env);
    dieIfFailed("xmlrpc_registry_new", env);

    xmlrpc_registry_add_method2(
        &env, registryP, "sample.add", &sample_add, NULL, NULL, NULL);
    dieIfFailed("xmlrpc_registry_add_method2", env);

    serverparm.config_file_name = NULL;
    serverparm.registryP        = registryP;
    serverparm.port_number      = atoi(argv[1]);

    xmlrpc_server_abyss_create(&env, &serverparm, XMLRPC_APSIZE(port_number),
                               &serverP);
    dieIfFailed("xmlrpc_server_abyss_create", env);
    
    xmlrpc_server_abyss_setup_sig(&env, serverP, &oldHandlersP);
    dieIfFailed("xmlrpc_server_abyss_setup_sig", env);

    setupSigtermHandler(serverP);

    printf("Running XML-RPC server...\n");

    xmlrpc_server_abyss_run_server(&env, serverP);
    dieIfFailed("xmlrpc_server_abyss_run_server", env);

    printf("Server has terminated\n");

    restoreSigtermHandler();
    xmlrpc_server_abyss_restore_sig(oldHandlersP);
    xmlrpc_server_abyss_destroy(serverP);
    xmlrpc_registry_free(registryP);
    xmlrpc_server_abyss_global_term();
    xmlrpc_env_clean(&env);

    return 0;
}
