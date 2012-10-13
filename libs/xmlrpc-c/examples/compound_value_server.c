/* An XML-RPC server program written in C, as an example of using
   compound XML-RPC values.

   For a simple server program that just deals with integer values,
   see xmlrpc_sample_add_server.c.  This example focuses just on the
   compound XML-RPC values and not the server functions.

   This server provides the example.divide XML-RPC method that the example
   client program compound_value_client.c invokes.  See that program for
   details on what the method does.

   The program takes one argument: the HTTP port number on which the server
   is to accept connections, in decimal.

   Example:

   $ ./compound_value_server 8080&
   $ ./compound_value_client
*/

#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#include "config.h"  /* information about this build environment */



static void
computeQuotient(xmlrpc_env *    const envP,
                xmlrpc_value *  const ratioP,
                xmlrpc_double * const quotientP) {

    xmlrpc_value * dividendP;

    xmlrpc_struct_find_value(envP, ratioP, "DIVIDEND", &dividendP);

    if (!envP->fault_occurred) {
        if (!dividendP)
            xmlrpc_env_set_fault(
                envP, 0, "Structure is missing 'DIVIDEND' member");
        else {
            xmlrpc_value * divisorP;

            xmlrpc_struct_find_value(envP, ratioP, "DIVISOR", &divisorP);

            if (!envP->fault_occurred) {
                if (!divisorP)
                    xmlrpc_env_set_fault(
                        envP, 0, "Structure is missing 'DIVISOR' member");
                else {
                    xmlrpc_double dividend;

                    xmlrpc_read_double(envP, dividendP, &dividend);

                    if (!envP->fault_occurred) {
                        xmlrpc_double divisor;

                        xmlrpc_read_double(envP, divisorP, &divisor);

                        if (!envP->fault_occurred)
                            *quotientP = dividend / divisor;
                    }
                    xmlrpc_DECREF(divisorP);
                }
            }
            xmlrpc_DECREF(dividendP);
        }
    }
}



static void
computeQuotients(xmlrpc_env *    const envP,
                 xmlrpc_value *  const ratioArrayP,
                 xmlrpc_value ** const quotientArrayPP) {

    xmlrpc_value * quotientArrayP;

    quotientArrayP = xmlrpc_array_new(envP);
    if (!envP->fault_occurred) {

        unsigned int const ratioCt = xmlrpc_array_size(envP, ratioArrayP);

        unsigned int i;

        for (i = 0; i < ratioCt && !envP->fault_occurred; ++i) {
            xmlrpc_value * ratioP;

            xmlrpc_array_read_item(envP, ratioArrayP, i, &ratioP);

            if (!envP->fault_occurred) {
                xmlrpc_double quotient;

                computeQuotient(envP, ratioP, &quotient);

                if (!envP->fault_occurred) {
                    xmlrpc_value * quotientP;

                    quotientP = xmlrpc_double_new(envP, quotient);

                    if (!envP->fault_occurred) {
                        xmlrpc_array_append_item(envP, quotientArrayP,
                                                 quotientP);
        
                        xmlrpc_DECREF(quotientP);
                    }
                }
                xmlrpc_DECREF(ratioP);
            }
        }
        if (envP->fault_occurred)
            xmlrpc_DECREF(quotientArrayP);
        else
            *quotientArrayPP = quotientArrayP;
    }
}



static xmlrpc_value *
example_divide(xmlrpc_env *   const envP,
               xmlrpc_value * const paramArrayP,
               void *         const serverInfo,
               void *         const channelInfo) {

    xmlrpc_value * retvalP;
    xmlrpc_int32 argVersion;
    xmlrpc_value * ratioArrayP;

    xmlrpc_decompose_value(envP, paramArrayP, "(iA)",
                           &argVersion, &ratioArrayP);
    if (envP->fault_occurred)
        return NULL;

    if (argVersion != 1) {
        xmlrpc_env_set_fault(envP, 0, "Parameter list version must be 1");
        return NULL;
    }

    computeQuotients(envP, ratioArrayP, &retvalP);

    xmlrpc_DECREF(ratioArrayP);

    if (envP->fault_occurred)
        return NULL;

    return retvalP;
}



int 
main(int           const argc, 
     const char ** const argv) {

    struct xmlrpc_method_info3 const methodInfo = {
        /* .methodName     = */ "example.divide",
        /* .methodFunction = */ &example_divide,
    };
    xmlrpc_server_abyss_parms serverparm;
    xmlrpc_registry * registryP;
    xmlrpc_env env;

    if (argc-1 != 1) {
        fprintf(stderr, "You must specify 1 argument:  The TCP port "
                "number on which the server will accept connections "
                "for RPCs (8080 is a common choice).  "
                "You specified %d arguments.\n",  argc-1);
        exit(1);
    }
    
    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method3(&env, registryP, &methodInfo);

    /* In the modern form of the Abyss API, we supply parameters in memory
       like a normal API.  We select the modern form by setting
       config_file_name to NULL: 
    */
    serverparm.config_file_name = NULL;
    serverparm.registryP        = registryP;
    serverparm.port_number      = atoi(argv[1]);
    serverparm.log_file_name    = "/tmp/xmlrpc_log";

    printf("Running XML-RPC server...\n");

    xmlrpc_server_abyss(&env, &serverparm, XMLRPC_APSIZE(log_file_name));

    /* xmlrpc_server_abyss() never returns */

    return 0;
}
