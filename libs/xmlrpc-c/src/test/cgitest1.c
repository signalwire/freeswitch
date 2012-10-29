/*============================================================================
  Act like a CGI script -- read POST data from Standard Input, interpret
  it as an XML-RPC call, and write an XML-RPC response to Standard Output.

  This is for use by a test program.
============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/server_cgi.h"

#include "testtool.h"


int total_tests;
int total_failures;



static xmlrpc_value *
sample_add(xmlrpc_env *   const env, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED) {

    xmlrpc_int32 x, y, z;

    /* Parse our argument array. */
    xmlrpc_decompose_value(env, param_array, "(ii)", &x, &y);
    if (env->fault_occurred)
        return NULL;

    /* Add our two numbers. */
    z = x + y;

    /* Return our result. */
    return xmlrpc_build_value(env, "i", z);
}



int
main(int     argc ATTR_UNUSED,
     char ** argv ATTR_UNUSED) {

    xmlrpc_env env;
    xmlrpc_registry * registryP;
    xmlrpc_value * argArrayP;

    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);
    TEST(registryP != NULL);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_add_method(&env, registryP, NULL, "sample.add",
                               sample_add, NULL);
    TEST_NO_FAULT(&env);

    argArrayP = xmlrpc_build_value(&env, "(ii)",
                                   (xmlrpc_int32) 25, (xmlrpc_int32) 17); 
    TEST_NO_FAULT(&env);

    /* The following reads from Standard Input and writes to Standard
       Output
    */
    xmlrpc_server_cgi_process_call(registryP);

    xmlrpc_DECREF(argArrayP);
    xmlrpc_registry_free(registryP);

    return 0;
}
