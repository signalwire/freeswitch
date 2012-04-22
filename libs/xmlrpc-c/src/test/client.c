#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "xmlrpc_config.h"
#include "transport_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/transport.h"

#include "test.h"
#include "client.h"


static void
testGlobalConst(void) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_client_setup_global_const(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_client_teardown_global_const();

    xmlrpc_client_setup_global_const(&env);
    TEST_NO_FAULT(&env);
    xmlrpc_client_setup_global_const(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_client_teardown_global_const();
    xmlrpc_client_teardown_global_const();

    xmlrpc_env_clean(&env);
}



static void
testCreateCurlParms(void) {
    
#if MUST_BUILD_CURL_CLIENT

    xmlrpc_env env;
    xmlrpc_client * clientP;
    struct xmlrpc_clientparms clientParms1;
    struct xmlrpc_curl_xportparms curlTransportParms1;

    xmlrpc_env_init(&env);

    clientParms1.transport = "curl";
    clientParms1.transportparmsP = &curlTransportParms1;

    curlTransportParms1.network_interface = "eth0";
    clientParms1.transportparm_size = XMLRPC_CXPSIZE(network_interface);
    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                         &clientParms1, XMLRPC_CPSIZE(transportparm_size),
                         &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    curlTransportParms1.no_ssl_verifypeer = 1;
    curlTransportParms1.no_ssl_verifyhost = 1;
    clientParms1.transportparm_size = XMLRPC_CXPSIZE(no_ssl_verifyhost);
    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                         &clientParms1, XMLRPC_CPSIZE(transportparm_size),
                         &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    curlTransportParms1.user_agent = "testprog/1.0";
    clientParms1.transportparm_size = XMLRPC_CXPSIZE(user_agent);
    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                         &clientParms1, XMLRPC_CPSIZE(transportparm_size),
                         &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    xmlrpc_env_clean(&env);
#endif  /* MUST_BUILD_CURL_CLIENT */
}



static void
testCreateSeparateXport(void) {

#if MUST_BUILD_CURL_CLIENT
    xmlrpc_env env;
    xmlrpc_client * clientP;
    struct xmlrpc_clientparms clientParms1;
    struct xmlrpc_curl_xportparms curlTransportParms1;
    struct xmlrpc_client_transport * transportP;

    xmlrpc_env_init(&env);

    xmlrpc_curl_transport_ops.create(
        &env, 0, "", "", &curlTransportParms1, 0,
        &transportP);

    TEST_NO_FAULT(&env);

    clientParms1.transport          = NULL;
    clientParms1.transportparmsP    = NULL;
    clientParms1.transportparm_size = 0;
    clientParms1.transportOpsP      = NULL;
    clientParms1.transportP         = NULL;
    
    xmlrpc_client_create(&env, 0, "", "",
                         &clientParms1, XMLRPC_CPSIZE(transportP),
                         &clientP);
    TEST_NO_FAULT(&env);

    clientParms1.transport = "curl";
    clientParms1.transportparmsP = &curlTransportParms1;
    clientParms1.transportparm_size = 0;
    clientParms1.transportOpsP = NULL;
    clientParms1.transportP = NULL;
    
    xmlrpc_client_create(&env, 0, "", "",
                         &clientParms1, XMLRPC_CPSIZE(transportP),
                         &clientP);
    TEST_NO_FAULT(&env);

    clientParms1.transportP = transportP;
    xmlrpc_client_create(&env, 0, "", "",
                         &clientParms1, XMLRPC_CPSIZE(transportP),
                         &clientP);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
        /* Both transportP and transport specified */

    clientParms1.transport          = NULL;
    clientParms1.transportparmsP    = NULL;
    clientParms1.transportparm_size = 0;
    clientParms1.transportOpsP      = &xmlrpc_curl_transport_ops;
    clientParms1.transportP         = transportP;

    xmlrpc_client_create(&env, 0, "", "",
                         &clientParms1, XMLRPC_CPSIZE(transportOpsP),
                         &clientP);

    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
        /* transportOpsP but no transportP */

    xmlrpc_client_create(&env, 0, "", "",
                         &clientParms1, XMLRPC_CPSIZE(transportP),
                         &clientP);

    TEST_NO_FAULT(&env);

    xmlrpc_curl_transport_ops.destroy(transportP);

    xmlrpc_env_clean(&env);

#endif  /* MUST_BUILD_CURL_CLIENT */
}



static void
testCreateDestroy(void) {

    xmlrpc_env env;
    xmlrpc_client * clientP;
    struct xmlrpc_clientparms clientParms1;
    struct xmlrpc_curl_xportparms curlTransportParms1;
    int interrupt;

    xmlrpc_env_init(&env);

    xmlrpc_client_create(&env, 0, "testprog", "1.0", NULL, 0, &clientP);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
        /* Didn't set up global const */

    xmlrpc_client_setup_global_const(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_client_create(&env, 0, "testprog", "1.0", NULL, 0, &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    xmlrpc_client_create(&env, 0, "testprog", "1.0", &clientParms1, 0,
                         &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    clientParms1.transport = "curl";
    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transport), &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    clientParms1.transportparmsP = NULL;
    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                         &clientParms1, XMLRPC_CPSIZE(transportparmsP),
                         &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    clientParms1.transportOpsP = NULL;
    clientParms1.transportP    = NULL;
    clientParms1.dialect       = xmlrpc_dialect_apache;
    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                         &clientParms1, XMLRPC_CPSIZE(dialect),
                         &clientP);
    TEST_NO_FAULT(&env);
    xmlrpc_client_destroy(clientP);

    clientParms1.transportparmsP = &curlTransportParms1;

    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                         &clientParms1, XMLRPC_CPSIZE(transportparmsP),
                         &clientP);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    clientParms1.transportparm_size = 0;
    xmlrpc_client_create(&env, 0, "testprog", "1.0",
                         &clientParms1, XMLRPC_CPSIZE(transportparm_size),
                         &clientP);
    TEST_NO_FAULT(&env);

    xmlrpc_client_set_interrupt(clientP, &interrupt);
    xmlrpc_client_set_interrupt(clientP, NULL);

    xmlrpc_client_destroy(clientP);

    testCreateCurlParms();

    testCreateSeparateXport();

    xmlrpc_client_teardown_global_const();

    xmlrpc_env_clean(&env);
}



static void
testSynchCall(void) {

    xmlrpc_env env;
    xmlrpc_client * clientP;
    xmlrpc_value * resultP;
    xmlrpc_value * emptyArrayP;
    xmlrpc_server_info * noSuchServerInfoP;

    xmlrpc_env_init(&env);

    emptyArrayP = xmlrpc_array_new(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_client_setup_global_const(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_client_create(&env, 0, "testprog", "1.0", NULL, 0, &clientP);
    TEST_NO_FAULT(&env);

    noSuchServerInfoP = xmlrpc_server_info_new(&env, "nosuchserver");
    TEST_NO_FAULT(&env);

    xmlrpc_client_call2(&env, clientP, noSuchServerInfoP, "nosuchmethod",
                        emptyArrayP, &resultP);
    TEST_FAULT(&env, XMLRPC_NETWORK_ERROR);  /* No such server */

    xmlrpc_client_call2f(&env, clientP, "nosuchserver", "nosuchmethod",
                          &resultP, "(i)", 7);
    TEST_FAULT(&env, XMLRPC_NETWORK_ERROR);  /* No such server */

    xmlrpc_server_info_free(noSuchServerInfoP);

    xmlrpc_client_destroy(clientP);

    xmlrpc_DECREF(emptyArrayP);
    
    xmlrpc_client_teardown_global_const();

    xmlrpc_env_clean(&env);
}



static void
testInitCleanup(void) {

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

    clientParms1.transportparmsP = &curlTransportParms1;

    /* Fails because we didn't include transportparm_size: */
    xmlrpc_client_init2(&env, 0, "testprog", "1.0",
                        &clientParms1, XMLRPC_CPSIZE(transportparmsP));
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

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

    xmlrpc_env_clean(&env);
}



static void
testServerInfo(void) {

    xmlrpc_env env;
    xmlrpc_server_info * serverInfoP;
    xmlrpc_server_info * serverInfo2P;

    printf("  Running serverInfo tests...\n");

    xmlrpc_env_init(&env);

    serverInfoP = xmlrpc_server_info_new(&env, "testurl");
    TEST_NO_FAULT(&env);

    serverInfo2P = xmlrpc_server_info_copy(&env, serverInfoP);
    TEST_NO_FAULT(&env);

    xmlrpc_server_info_free(serverInfo2P);

    /* Fails because we haven't set user/password yet: */
    xmlrpc_server_info_allow_auth_basic(&env, serverInfoP);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    
    xmlrpc_server_info_set_basic_auth(&env, serverInfoP,
                                      "username", "password");
    TEST_NO_FAULT(&env);

    xmlrpc_server_info_set_user(&env, serverInfoP, "username", "password");
    TEST_NO_FAULT(&env);

    xmlrpc_server_info_allow_auth_basic(&env, serverInfoP);
    TEST_NO_FAULT(&env);
    
    xmlrpc_server_info_disallow_auth_basic(&env, serverInfoP);
    TEST_NO_FAULT(&env);
    
    xmlrpc_server_info_allow_auth_digest(&env, serverInfoP);
    TEST_NO_FAULT(&env);
    
    xmlrpc_server_info_disallow_auth_digest(&env, serverInfoP);
    TEST_NO_FAULT(&env);
    
    xmlrpc_server_info_allow_auth_negotiate(&env, serverInfoP);
    TEST_NO_FAULT(&env);
    
    xmlrpc_server_info_disallow_auth_negotiate(&env, serverInfoP);
    TEST_NO_FAULT(&env);
    
    xmlrpc_server_info_allow_auth_ntlm(&env, serverInfoP);
    TEST_NO_FAULT(&env);
    
    xmlrpc_server_info_disallow_auth_ntlm(&env, serverInfoP);
    TEST_NO_FAULT(&env);

    xmlrpc_server_info_free(serverInfoP);
    
    xmlrpc_env_clean(&env);
}



void 
test_client(void) {

    printf("Running client tests.");

    testGlobalConst();
    testCreateDestroy();
    testInitCleanup();
    testServerInfo();
    testSynchCall();

    printf("\n");
    printf("Client tests done.\n");
}
