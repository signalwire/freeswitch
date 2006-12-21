#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"

#include "test.h"
#include "client.h"



static void
test_init_cleanup(void) {

    xmlrpc_env env;
    struct xmlrpc_clientparms clientParms1;
    struct xmlrpc_curl_xportparms curlTransportParms1;

    xmlrpc_env_init(&env);

    xmlrpc_client_init2(&env, 0, "testprog", "1.0", NULL, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    xmlrpc_client_init2(&env, 0, "testprog", "1.0", &clientParms1, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    clientParms1.transport = "curl";
    xmlrpc_client_init2(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transport));
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    clientParms1.transportparmsP = NULL;
    xmlrpc_client_init2(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transportparmsP));
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    clientParms1.transportparmsP = (struct xmlrpc_xportparms *)
        &curlTransportParms1;
    {
        xmlrpc_env env2;
        xmlrpc_env_init(&env2);
        xmlrpc_client_init2(&env2, 0, "testprog", "1.0",
                            &clientParms1, XMLRPC_CPSIZE(transportparmsP));
        TEST_FAULT(&env2, XMLRPC_INTERNAL_ERROR);
    }
    clientParms1.transportparm_size = 0;
    xmlrpc_client_init2(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transportparm_size));
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    curlTransportParms1.network_interface = "eth0";
    clientParms1.transportparm_size = XMLRPC_CXPSIZE(network_interface);
    xmlrpc_client_init2(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transportparm_size));
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    curlTransportParms1.no_ssl_verifypeer = 1;
    curlTransportParms1.no_ssl_verifyhost = 1;
    clientParms1.transportparm_size = XMLRPC_CXPSIZE(no_ssl_verifyhost);
    xmlrpc_client_init2(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transportparm_size));
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    curlTransportParms1.user_agent = "testprog/1.0";
    clientParms1.transportparm_size = XMLRPC_CXPSIZE(user_agent);
    xmlrpc_client_init2(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transportparm_size));
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();

    xmlrpc_client_init(0, "testprog", "1.0");
    TEST_NO_FAULT(&env);
    xmlrpc_client_cleanup();
}



void 
test_client(void) {

    printf("Running client tests.");

    test_init_cleanup();

    printf("\n");
    printf("Client tests done.\n");
}
