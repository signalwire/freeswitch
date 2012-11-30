#include "unistdx.h"
#include <stdio.h>
#include "bool.h"

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/abyss.h"
#include "xmlrpc-c/server_abyss.h"

#include "testtool.h"

#include "server_abyss.h"


static xmlrpc_call_processor myXmlProcessor;

static void
myXmlProcessor(xmlrpc_env *        const envP ATTR_UNUSED,
               void *              const processorArg ATTR_UNUSED,
               const char *        const callXml ATTR_UNUSED,
               size_t              const callXmlLen ATTR_UNUSED,
               TSession *          const abyssSessionP ATTR_UNUSED, 
               xmlrpc_mem_block ** const responseXmlPP ATTR_UNUSED) {

    printf("XML processor running\n");
}



static void
testSetHandlers(TServer * const abyssServerP) {

    xmlrpc_env env;
    xmlrpc_registry * registryP;
    xmlrpc_server_abyss_handler_parms parms;

    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);
    TEST_NO_FAULT(&env);
    TEST(registryP != NULL);

    parms.xml_processor = &myXmlProcessor;
    parms.xml_processor_arg = NULL;
    parms.xml_processor_max_stack = 512;
    parms.uri_path = "/RPC6";
    parms.chunk_response = true;
    parms.allow_origin = "*";

    xmlrpc_server_abyss_set_handler3(
        &env, abyssServerP, &parms, XMLRPC_AHPSIZE(xml_processor_arg));
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);  /* Parms too short */
    xmlrpc_server_abyss_set_handler3(
        &env, abyssServerP, &parms, XMLRPC_AHPSIZE(allow_origin));
    TEST_NO_FAULT(&env);

    xmlrpc_server_abyss_set_handler2(abyssServerP, "/RPC5",
                                     &myXmlProcessor, NULL, 512, true);

    xmlrpc_server_abyss_set_handler(&env, abyssServerP, "/RPC3", registryP);
    TEST_NO_FAULT(&env);

    xmlrpc_server_abyss_set_handlers2(abyssServerP, "/RPC4", registryP);

    xmlrpc_server_abyss_set_handlers(abyssServerP, registryP);
    
    xmlrpc_server_abyss_set_default_handler(abyssServerP);

    xmlrpc_registry_free(registryP);

    {
        xmlrpc_registry * registryP;
        registryP = xmlrpc_registry_new(&env);
        xmlrpc_server_abyss_set_handlers(abyssServerP, registryP);
        xmlrpc_registry_free(registryP);
    }
    xmlrpc_env_clean(&env);
}



static void
testServerParms(void) {
    xmlrpc_server_abyss_parms parms;

    parms.config_file_name = NULL;
    parms.registryP = NULL;
    parms.port_number = 1000;
    parms.log_file_name = "/tmp/xmlrpc_logfile";
    parms.keepalive_timeout = 5;
    parms.keepalive_max_conn = 4;
    parms.timeout = 50;
    parms.dont_advertise = TRUE;
    parms.uri_path = "/RPC9";
    parms.chunk_response = TRUE;
    parms.allow_origin = "*";
};



static void
testObject(void) {

    xmlrpc_env env;
    xmlrpc_server_abyss_parms parms;
    xmlrpc_server_abyss_t * serverP;
    xmlrpc_registry * registryP;
    xmlrpc_server_abyss_sig * oldHandlersP;

    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);
    TEST_NO_FAULT(&env);

    parms.config_file_name = NULL;
    parms.registryP = registryP;

    serverP = NULL;

    xmlrpc_server_abyss_create(&env, &parms, XMLRPC_APSIZE(registryP),
                               &serverP);

    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);  /* Global init not done */

    xmlrpc_server_abyss_global_init(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_server_abyss_create(&env, &parms, XMLRPC_APSIZE(registryP),
                               &serverP);

    TEST_NO_FAULT(&env);
    TEST(serverP != NULL);

    xmlrpc_server_abyss_terminate(&env, serverP);
    TEST_NO_FAULT(&env);

    xmlrpc_server_abyss_reset_terminate(&env, serverP);
    TEST_NO_FAULT(&env);

    xmlrpc_server_abyss_setup_sig(&env, serverP, &oldHandlersP);
    TEST_NO_FAULT(&env);

    xmlrpc_server_abyss_use_sigchld(serverP);
    
    xmlrpc_server_abyss_restore_sig(oldHandlersP);
    TEST_NO_FAULT(&env);

    free(oldHandlersP);

    xmlrpc_server_abyss_destroy(serverP);
    
    xmlrpc_registry_free(registryP);

    xmlrpc_server_abyss_global_term();

    xmlrpc_server_abyss_setup_sig(&env, serverP, &oldHandlersP);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR); /* Not globally initialized */

    xmlrpc_env_clean(&env);
}



void
test_server_abyss(void) {

    TServer abyssServer;

    printf("Running Abyss XML-RPC server tests...\n");

    ServerCreate(&abyssServer, "testserver", 8080, NULL, NULL);
    
    testSetHandlers(&abyssServer);

    ServerSetKeepaliveTimeout(&abyssServer, 60);
    ServerSetKeepaliveMaxConn(&abyssServer, 10);
    ServerSetTimeout(&abyssServer, 0);
    ServerSetAdvertise(&abyssServer, FALSE);

    ServerFree(&abyssServer);

    testServerParms();

    testObject();

    printf("\n");
    printf("Abyss XML-RPC server tests done.\n");
}
