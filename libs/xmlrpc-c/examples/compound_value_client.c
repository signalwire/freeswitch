/* An XML-RPC client program written in C, as an example of using
   compound XML-RPC values.

   For a simple client program that just deals with integer values,
   see xmlrpc_sample_add_client.c.  This example focuses just on the
   compound XML-RPC values and not the client functions.

   This client invokes the example.divide XML-RPC method that the example
   server program compound_value_server.c provides.  That method takes a
   list of pairs of numbers and returns the list of their quotients.

   Compound XML-RPC values are arrays and structures.  We call them compound
   because they are made up of other XML-RPC values (e.g. an array of XML-RPC
   integers).

   The arguments to the example.divide method are specified as follows:

   There are two arguments:

     Argument 0: Integer.  Version number of this argument protocol.  Must
                 be 1.


     Argument 1: Array.  One element for each pair of numbers you want the
                 server to divide.  Each element is structure, with these
                 members:

                 KEY: "dividend"
                 VALUE: floating point number.  The dividend.

                 KEY: "divisor"
                 VALUE: floating point number.  The divisor.

   The result of the method is an array.  It has one member for each pair of
   numbers in the arguments (So it is the same size as Argument 1).  That
   member is a floating point number.  It is the quotient of the numbers
   in the corresponding element of Argument 1.

   The client sends the RPC to the server running on the local system
   ("localhost"), HTTP Port 8080.
*/

#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

#define NAME "Xmlrpc-c Test Client"
#define VERSION "1.0"

static void 
dieIfFaultOccurred (xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "ERROR: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



struct ratio {
    double dividend;
    double divisor;
};



int 
main(int           const argc, 
     const char ** const argv) {

    const char * const serverUrl = "http://localhost:8080/RPC2";
    const char * const methodName = "example.divide";
    unsigned int const argVersion = 1;
    struct ratio const data[] = {{1,2},{12,3},{10,3},{89,3000}};
    xmlrpc_env env;
    xmlrpc_value * resultP;
    unsigned int i;
    xmlrpc_value * ratioArrayP;
    unsigned int quotientCt;

    if (argc-1 > 0) {
        fprintf(stderr, "This program has no arguments\n");
        exit(1);
    }

    xmlrpc_env_init(&env);

    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
    dieIfFaultOccurred(&env);

    /* Build the 2nd method argument: the array of ratios */

    ratioArrayP = xmlrpc_array_new(&env);
    dieIfFaultOccurred(&env);

    for (i = 0; i < 4; ++i) {
        xmlrpc_value * dividendP;
        xmlrpc_value * divisorP;
        xmlrpc_value * ratioP;

        dividendP = xmlrpc_double_new(&env, data[i].dividend);
        dieIfFaultOccurred(&env);
        divisorP  = xmlrpc_double_new(&env, data[i].divisor);
        dieIfFaultOccurred(&env);

        ratioP = xmlrpc_struct_new(&env);
        dieIfFaultOccurred(&env);

        xmlrpc_struct_set_value(&env, ratioP, "DIVIDEND", dividendP);
        dieIfFaultOccurred(&env);
        xmlrpc_struct_set_value(&env, ratioP, "DIVISOR",  divisorP);
        dieIfFaultOccurred(&env);
        
        xmlrpc_array_append_item(&env, ratioArrayP, ratioP);
        dieIfFaultOccurred(&env);

        xmlrpc_DECREF(ratioP);
        xmlrpc_DECREF(divisorP);
        xmlrpc_DECREF(dividendP);
    }        

    /* Make the call */

    resultP = xmlrpc_client_call(&env, serverUrl, methodName, "(iA)",
                                 (xmlrpc_int32) argVersion, ratioArrayP);
    dieIfFaultOccurred(&env);
    
    /* Print out the quotients returned */

    quotientCt = xmlrpc_array_size(&env, resultP);
    dieIfFaultOccurred(&env);

    for (i = 0; i < quotientCt; ++i) {
        xmlrpc_value * quotientP;
        xmlrpc_double quotient;

        xmlrpc_array_read_item(&env, resultP, i, &quotientP);
        dieIfFaultOccurred(&env);

        xmlrpc_read_double(&env, quotientP, &quotient);
        dieIfFaultOccurred(&env);

        printf("Server says quotient %u is %f\n", i, quotient);

        xmlrpc_DECREF(quotientP);
    }

    xmlrpc_DECREF(resultP);

    xmlrpc_env_clean(&env);
    
    xmlrpc_client_cleanup();

    return 0;
}

