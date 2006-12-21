#include <stdio.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/abyss.h"
#include "xmlrpc-c/server_abyss.h"

#include "test.h"

#include "server_abyss.h"


void
test_server_abyss(void) {


    xmlrpc_env env;
    xmlrpc_registry * registryP;
    TServer abyssServer;

    printf("Running Abyss server tests...\n");

    xmlrpc_env_init(&env);

    registryP = xmlrpc_registry_new(&env);
    
    TEST(registryP != NULL);
    TEST_NO_FAULT(&env);

    ServerCreate(&abyssServer, "testserver", 8080, NULL, NULL);
    
    xmlrpc_server_abyss_set_handlers(&abyssServer, registryP);

    xmlrpc_server_abyss_set_handler(&env, &abyssServer, "/RPC3", registryP);

    TEST_NO_FAULT(&env);

    ServerFree(&abyssServer);

    printf("\n");
    printf("Abyss server tests done.\n");
}
