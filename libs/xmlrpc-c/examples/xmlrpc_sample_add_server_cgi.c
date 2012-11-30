/* A CGI script written in C to effect a simple XML-RPC server.

   Example of use:

     - Compile this as the executable 'xmlrpc_sample_add_server.cgi'

     - Place the .cgi file in web server www.example.com's /cgi-bin
       directory.

     - Configure the web server to permit CGI scripts in /cgi-bin
       (Apache ExecCgi directory option).

     - Configure the web server to recognize this .cgi file as a CGI
       script (Apache "AddHandler cgi-script ..." or ScriptAlias).

     - $ xmlrpc http://www.example.com/cgi-bin/xmlrpc_sample_add_server.cgi \
           sample.add i/5 i/7
*/


#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_cgi.h>

#include "config.h"  /* information about this build environment */

static xmlrpc_value *
sample_add(xmlrpc_env *   const envP,
           xmlrpc_value * const paramArrayP,
           void *         const user_data) {

    xmlrpc_int32 x, y, z;

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

    xmlrpc_registry * registryP;
    xmlrpc_env env;

    if (argc-1 > 0 && argv==argv) {
        fprintf(stderr, "There are no arguments to a CGI script\n");
        exit(1);
    }

    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method(
        &env, registryP, NULL, "sample.add", &sample_add, NULL);

    xmlrpc_server_cgi_process_call(registryP);

    xmlrpc_registry_free(registryP);

    return 0;
}
