#include <stdlib.h>
#include <string.h>

#include "int.h"
#include "casprintf.h"
#include "girstring.h"

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"

#include "testtool.h"
#include "xml_data.h"
#include "method_registry.h"


#define FOO_SERVERINFO     ((void*) 0xF00)
#define FOO_CALLINFO       ((void*) 0xFC0)
#define BAR_SERVERINFO     ((void*) 0xBAF)
#define BAR_CALLINFO       ((void*) 0xBAC)
#define MULTI_CALLINFO     ((void*) 0xFFF)
#define DEFAULT_SERVERINFO ((void*) 0xD00)
#define DEFAULT_CALLINFO   ((void*) 0xDC0)

static const char * const barHelp = "This is the help for Method test.bar.";



static void
testVersion(void) {

    unsigned int major, minor, point;

    xmlrpc_server_version(&major, &minor, &point);

#ifndef WIN32    
    /* xmlrpc_server_version_major, etc. are not exported from a Windows DLL */

    TEST(major = xmlrpc_server_version_major);
    TEST(minor = xmlrpc_server_version_minor);
    TEST(point = xmlrpc_server_version_point);
#endif
}



static xmlrpc_value *
test_foo(xmlrpc_env *   const envP,
         xmlrpc_value * const paramArrayP,
         void *         const serverInfo,
         void *         const callInfo) {

    xmlrpc_int32 x, y;

    TEST_NO_FAULT(envP);
    TEST(paramArrayP != NULL);
    TEST(serverInfo == FOO_SERVERINFO);
    TEST(callInfo == FOO_CALLINFO || callInfo == MULTI_CALLINFO);

    xmlrpc_decompose_value(envP, paramArrayP, "(ii)", &x, &y);
    TEST_NO_FAULT(envP);
    TEST(x == 25);
    TEST(y == 17);

    return xmlrpc_build_value(envP, "i", (xmlrpc_int32) x + y);
}



static xmlrpc_value *
test_foo_type1(xmlrpc_env *   const envP,
               xmlrpc_value * const paramArrayP,
               void *         const serverInfo) {
    
    xmlrpc_int32 x, y;

    TEST_NO_FAULT(envP);
    TEST(paramArrayP != NULL);
    TEST(serverInfo == FOO_SERVERINFO);

    xmlrpc_decompose_value(envP, paramArrayP, "(ii)", &x, &y);
    TEST_NO_FAULT(envP);
    TEST(x == 25);
    TEST(y == 17);

    return xmlrpc_build_value(envP, "i", (xmlrpc_int32) x + y);
}



static xmlrpc_value *
test_bar(xmlrpc_env *   const envP,
         xmlrpc_value * const paramArrayP,
         void *         const serverInfo,
         void *         const callInfo) {

    xmlrpc_int32 x, y;

    TEST_NO_FAULT(envP);
    TEST(paramArrayP != NULL);
    TEST(serverInfo == BAR_SERVERINFO);
    TEST(callInfo == BAR_CALLINFO || callInfo == MULTI_CALLINFO);

    xmlrpc_decompose_value(envP, paramArrayP, "(ii)", &x, &y);
    TEST_NO_FAULT(envP);
    TEST(x == 25);
    TEST(y == 17);

    xmlrpc_env_set_fault(envP, 123, "Test fault");

    return NULL;
}



static xmlrpc_value *
test_default(xmlrpc_env *   const envP,
             const char *   const callInfo,
             const char *   const methodName,
             xmlrpc_value * const paramArrayP,
             void *         const serverInfo) {

    xmlrpc_int32 x, y;

    TEST_NO_FAULT(envP);
    TEST(paramArrayP != NULL);
    TEST(serverInfo == DEFAULT_SERVERINFO);

    TEST(streq(methodName, "test.nosuch") ||
         streq(methodName, "test.nosuch.old"));

    if (streq(methodName, "nosuch.method"))
        TEST(callInfo == DEFAULT_CALLINFO);
    else if (streq(methodName, "nosuch.method.old"))
        TEST(callInfo == NULL);

    xmlrpc_decompose_value(envP, paramArrayP, "(ii)", &x, &y);
    TEST_NO_FAULT(envP);
    TEST(x == 25);
    TEST(y == 17);

    return xmlrpc_build_value(envP, "i", 2 * (x + y));
}



static xmlrpc_value *
test_exttype(xmlrpc_env *   const envP,
             xmlrpc_value * const paramArrayP ATTR_UNUSED,
             void *         const serverInfo ATTR_UNUSED,
             void *         const callInfo ATTR_UNUSED) {

    return xmlrpc_build_value(envP, "(In)", (xmlrpc_int64)8);
}



static void
doRpc(xmlrpc_env *      const envP,
      xmlrpc_registry * const registryP,
      const char *      const methodName,
      xmlrpc_value *    const argArrayP,
      void *            const callInfo,
      xmlrpc_value **   const resultPP) {
/*----------------------------------------------------------------------------
   Do what an XML-RPC server would do -- pass an XML call to the registry
   and get XML back.

   Actually to our caller, we look more like an Xmlrpc-c client.  We're
   both the client and the server all bound together.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * callP;
    xmlrpc_mem_block * responseP;

    /* Build a call, and tell the registry to handle it. */
    callP = xmlrpc_mem_block_new(envP, 0);
    TEST_NO_FAULT(envP);
    xmlrpc_serialize_call(envP, callP, methodName, argArrayP);
    TEST_NO_FAULT(envP);

    if (callInfo)
        xmlrpc_registry_process_call2(
            envP, registryP,
            xmlrpc_mem_block_contents(callP),
            xmlrpc_mem_block_size(callP),
            callInfo, &responseP);
    else
        responseP = xmlrpc_registry_process_call(
            envP, registryP, NULL,
            xmlrpc_mem_block_contents(callP),
            xmlrpc_mem_block_size(callP));

    TEST_NO_FAULT(envP);
    TEST(responseP != NULL);

    /* Parse the response. */
    *resultPP = xmlrpc_parse_response(envP,
                                      xmlrpc_mem_block_contents(responseP),
                                      xmlrpc_mem_block_size(responseP));

    xmlrpc_mem_block_free(callP);
    xmlrpc_mem_block_free(responseP);
}



static const char * const validSigString[] = {
    "i:",
    "s:d",
    "i:bds86SA",
    "i:,i:",
    "i:dd,s:,A:A",
    "i:,",
    "b:i,",
    "b:i,b:,",
    NULL
};

static const char * const invalidSigString[] = {
    "",
    "i",
    "q",
    "i:q",
    "i:ddq",
    ",",
    ",i:",
    "i,",
    "b:i,,b:i",
    "ii:",
    "ii:ii",
    NULL
};



static void
test_system_methodSignature(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Test system.methodSignature system method.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * argArrayP;
    xmlrpc_value * resultP;
    const char * type0;
    const char * type1;
    const char * type2;
    const char * type3;
    const char * type4;
    const char * type5;
    const char * type6;
    const char * type7;
    const char * nosigstring;

    xmlrpc_env_init(&env);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.nosuchmethod");
    doRpc(&env, registryP, "system.methodSignature", argArrayP, NULL,
          &resultP);
    TEST_FAULT(&env, XMLRPC_NO_SUCH_METHOD_ERROR);
    xmlrpc_DECREF(argArrayP);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.nosig0");

    doRpc(&env, registryP, "system.methodSignature", argArrayP, NULL,
          &resultP);
    TEST_NO_FAULT(&env);

    xmlrpc_read_string(&env, resultP, &nosigstring);
    TEST_NO_FAULT(&env);
    
    TEST(streq(nosigstring, "undef"));
    strfree(nosigstring);
    xmlrpc_DECREF(resultP);
    xmlrpc_DECREF(argArrayP);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.validsig0");
    doRpc(&env, registryP, "system.methodSignature", argArrayP, NULL,
          &resultP);
    TEST_NO_FAULT(&env);

    xmlrpc_decompose_value(&env, resultP, "((s))", &type0);
    TEST_NO_FAULT(&env);
    TEST(streq(type0, "int"));
    strfree(type0);
    xmlrpc_DECREF(resultP);
    xmlrpc_DECREF(argArrayP);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.validsig2");
    doRpc(&env, registryP, "system.methodSignature", argArrayP, NULL,
          &resultP);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, resultP, "((ssssssss))",
                           &type0, &type1, &type2, &type3,
                           &type4, &type5, &type6, &type7);
    TEST_NO_FAULT(&env);
    TEST(streq(type0, "int"));
    TEST(streq(type1, "boolean"));
    TEST(streq(type2, "double"));
    TEST(streq(type3, "string"));
    TEST(streq(type4, "dateTime.iso8601"));
    TEST(streq(type5, "base64"));
    TEST(streq(type6, "struct"));
    TEST(streq(type7, "array"));
    strfree(type0); strfree(type1); strfree(type2); strfree(type3);
    strfree(type4); strfree(type5); strfree(type6); strfree(type7);
    xmlrpc_DECREF(resultP);
    xmlrpc_DECREF(argArrayP);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.validsig3");
    doRpc(&env, registryP, "system.methodSignature", argArrayP, NULL,
          &resultP);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, resultP, "((s)(s))", &type0, &type1);

    TEST_NO_FAULT(&env);
    TEST(streq(type0, "int"));
    TEST(streq(type1, "int"));
    strfree(type0);
    strfree(type1);
    xmlrpc_DECREF(resultP);
    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);
}



static void
test_signature(void) {

    xmlrpc_env env;
    xmlrpc_registry * registryP;
    unsigned int i;

    xmlrpc_env_init(&env);

    printf("  Running signature tests.");

    registryP = xmlrpc_registry_new(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_add_method2(&env, registryP, "test.nosig0",
                                test_foo, NULL, NULL, FOO_SERVERINFO);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_add_method2(&env, registryP, "test.nosig1",
                                test_foo, "?", NULL, FOO_SERVERINFO);
    TEST_NO_FAULT(&env);

    for (i = 0; validSigString[i]; ++i) {
        const char * methodName;
        casprintf(&methodName, "test.validsig%u", i);
        xmlrpc_registry_add_method2(&env, registryP, methodName,
                                    test_foo,
                                    validSigString[i], NULL, FOO_SERVERINFO);
        TEST_NO_FAULT(&env);
        strfree(methodName);
    }

    for (i = 0; invalidSigString[i]; ++i) {
        const char * methodName;
        casprintf(&methodName, "test.invalidsig%u", i);
        xmlrpc_registry_add_method2(&env, registryP, methodName,
                                    test_foo,
                                    invalidSigString[i], NULL, FOO_SERVERINFO);
        TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
        strfree(methodName);
    }

    xmlrpc_registry_add_method_w_doc(&env, registryP, NULL, "test.old",
                                     test_foo_type1, FOO_SERVERINFO,
                                     NULL, NULL);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_add_method_w_doc(&env, registryP, NULL, "test.old.invalid",
                                     test_foo_type1, FOO_SERVERINFO,
                                     invalidSigString[0], NULL);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    test_system_methodSignature(registryP);

    xmlrpc_registry_free(registryP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void
test_disable_introspection(void) {

    xmlrpc_env env;
    xmlrpc_registry * registryP;
    xmlrpc_value * argArrayP;
    xmlrpc_value * resultP;

    xmlrpc_env_init(&env);

    printf("  Running disable introspection tests.");

    registryP = xmlrpc_registry_new(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_add_method2(&env, registryP, "test.nosig0",
                                test_foo, NULL, NULL, FOO_SERVERINFO);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_disable_introspection(registryP);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.nosig0");
    doRpc(&env, registryP, "system.methodSignature", argArrayP, NULL,
          &resultP);
    TEST_FAULT(&env, XMLRPC_INTROSPECTION_DISABLED_ERROR);
    xmlrpc_DECREF(argArrayP);
    
    xmlrpc_registry_free(registryP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static const char * const expectedMethodName[] = {
/*----------------------------------------------------------------------------
   The list we expect back from system.listMethods.
-----------------------------------------------------------------------------*/
    "system.listMethods",
    "system.methodExist",
    "system.methodHelp",
    "system.methodSignature",
    "system.multicall",
    "system.shutdown",
    "system.capabilities",
    "system.getCapabilities",
    "test.foo",
    "test.bar"
};



static void
test_system_listMethods(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Test system.listMethods
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_value * argArrayP;
    const char * methodName[ARRAY_SIZE(expectedMethodName)];
    unsigned int size;
    unsigned int i;

    xmlrpc_env_init(&env);

    printf("  Running system.listMethods tests.");

    argArrayP = xmlrpc_array_new(&env);
    TEST_NO_FAULT(&env);

    doRpc(&env, registryP, "system.listMethods", argArrayP, NULL, &resultP);
    TEST_NO_FAULT(&env);

    TEST(xmlrpc_value_type(resultP) == XMLRPC_TYPE_ARRAY);

    size = xmlrpc_array_size(&env, resultP);

    TEST_NO_FAULT(&env);

    TEST(size == ARRAY_SIZE(expectedMethodName));

    xmlrpc_decompose_value(&env, resultP, "(ssssssssss)",
                           &methodName[0], &methodName[1],
                           &methodName[2], &methodName[3],
                           &methodName[4], &methodName[5],
                           &methodName[6], &methodName[7],
                           &methodName[8], &methodName[9]);

    TEST_NO_FAULT(&env);

    for (i = 0; i < ARRAY_SIZE(expectedMethodName); ++i) {
        TEST(streq(methodName[i], expectedMethodName[i]));
        strfree(methodName[i]);
    }

    xmlrpc_DECREF(resultP);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void
test_system_methodExist(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Test system.methodExist
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_value * argArrayP;
    xmlrpc_bool exists;

    xmlrpc_env_init(&env);

    printf("  Running system.methodExist tests.");

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.foo");
    TEST_NO_FAULT(&env);

    doRpc(&env, registryP, "system.methodExist", argArrayP, NULL, &resultP);
    TEST_NO_FAULT(&env);

    TEST(xmlrpc_value_type(resultP) == XMLRPC_TYPE_BOOL);

    xmlrpc_read_bool(&env, resultP, &exists);
    TEST_NO_FAULT(&env);

    TEST(exists);

    xmlrpc_DECREF(resultP);

    xmlrpc_DECREF(argArrayP);


    argArrayP = xmlrpc_build_value(&env, "(s)", "nosuchmethod");
    TEST_NO_FAULT(&env);

    doRpc(&env, registryP, "system.methodExist", argArrayP, NULL, &resultP);
    TEST_NO_FAULT(&env);

    TEST(xmlrpc_value_type(resultP) == XMLRPC_TYPE_BOOL);

    xmlrpc_read_bool(&env, resultP, &exists);
    TEST_NO_FAULT(&env);

    TEST(!exists);

    xmlrpc_DECREF(resultP);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void 
testNoHelp(xmlrpc_registry * const registryP) {

    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_value * argArrayP;
    const char * helpString;

    xmlrpc_env_init(&env);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.foo");
    TEST_NO_FAULT(&env);

    doRpc(&env, registryP, "system.methodHelp", argArrayP, NULL, &resultP);
    TEST_NO_FAULT(&env);

    TEST(xmlrpc_value_type(resultP) == XMLRPC_TYPE_STRING);

    xmlrpc_read_string(&env, resultP, &helpString);
    TEST_NO_FAULT(&env);

    TEST(streq(helpString, "No help is available for this method."));

    strfree(helpString);

    xmlrpc_DECREF(resultP);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);
}



static void 
testExistentHelp(xmlrpc_registry * const registryP) {

    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_value * argArrayP;
    const char * helpString;

    xmlrpc_env_init(&env);

    argArrayP = xmlrpc_build_value(&env, "(s)", "test.bar");
    TEST_NO_FAULT(&env);

    doRpc(&env, registryP, "system.methodHelp", argArrayP, NULL, &resultP);
    TEST_NO_FAULT(&env);

    TEST(xmlrpc_value_type(resultP) == XMLRPC_TYPE_STRING);

    xmlrpc_read_string(&env, resultP, &helpString);
    TEST_NO_FAULT(&env);

    TEST(streq(helpString, barHelp));

    strfree(helpString);

    xmlrpc_DECREF(resultP);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);
}



static void
test_system_methodHelp(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Test system.methodHelp
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    printf("  Running system.methodHelp tests.");

    testNoHelp(registryP);

    testExistentHelp(registryP);

    printf("\n");
}



static void
test_system_capabilities(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Test system.capabilities
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_value * argArrayP;
    const char * facility;
    xmlrpc_int version_major, version_minor, version_point;
    xmlrpc_int protocol_version;

    xmlrpc_env_init(&env);

    printf("  Running system.capabilities tests.");

    argArrayP = xmlrpc_array_new(&env);
    TEST_NO_FAULT(&env);

    doRpc(&env, registryP, "system.capabilities", argArrayP, NULL, &resultP);
    TEST_NO_FAULT(&env);

    xmlrpc_decompose_value(&env, resultP, "{s:s,s:i,s:i,s:i,s:i,*}",
                           "facility", &facility,
                           "version_major", &version_major,
                           "version_minor", &version_minor,
                           "version_point", &version_point,
                           "protocol_version", &protocol_version);
    TEST_NO_FAULT(&env);

    TEST(streq(facility, "xmlrpc-c"));
    TEST(protocol_version == 2);

    strfree(facility);

    xmlrpc_DECREF(resultP);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void
test_system_getCapabilities(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Test system.getCapabilities
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_value * argArrayP;
    const char * specUrl;
    int specVersion;

    xmlrpc_env_init(&env);

    printf("  Running system.getCapabilities tests.");

    argArrayP = xmlrpc_array_new(&env);
    TEST_NO_FAULT(&env);

    doRpc(&env, registryP, "system.getCapabilities", argArrayP, NULL,
          &resultP);
    TEST_NO_FAULT(&env);

    xmlrpc_decompose_value(&env, resultP, "{s:{s:s,s:i,*},*}",
                           "introspect",
                           "specUrl", &specUrl,
                           "specVersion", &specVersion);
    TEST_NO_FAULT(&env);

    TEST(streq(specUrl,
               "http://xmlrpc-c.sourceforge.net/xmlrpc-c/introspection.html"));
    TEST(specVersion == 1);

    strfree(specUrl);

    xmlrpc_DECREF(resultP);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void
test_system_multicall(xmlrpc_registry * const registryP) {
/*----------------------------------------------------------------------------
   Test system.multicall
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * multiP;
    xmlrpc_int32 foo1_result, foo2_result;
    xmlrpc_int32 bar_code, nosuch_code;
    char *bar_string, *nosuch_string;
    xmlrpc_value * valueP;
    xmlrpc_value * argArrayP;

    xmlrpc_env_init(&env);

    printf("  Running multicall tests.");

    /* Build an argument array for our calls. */
    argArrayP = xmlrpc_build_value(&env, "(ii)",
                                   (xmlrpc_int32) 25, (xmlrpc_int32) 17); 
    TEST_NO_FAULT(&env);

    multiP = xmlrpc_build_value(&env,
                                "(("
                                "{s:s,s:A}"   /* test.foo */
                                "{s:s,s:A}"   /* test.bar */
                                "{s:s,s:A}"   /* test.nosuch */
                                "{s:s,s:A}"   /* test.foo */
                                "))",
                                "methodName", "test.foo",
                                "params", argArrayP,
                                "methodName", "test.bar",
                                "params", argArrayP,
                                "methodName", "test.nosuch",
                                "params", argArrayP,
                                "methodName", "test.foo",
                                "params", argArrayP);
    TEST_NO_FAULT(&env);    
    doRpc(&env, registryP, "system.multicall", multiP, MULTI_CALLINFO,
          &valueP);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, valueP,
                           "("
                           "(i)"           /* result of test.foo */
                           "{s:i,s:s,*}"   /* result of test.bar */
                           "{s:i,s:s,*}"   /* result of test.nosuch */
                           "(i)"           /* result of test.foo #2 */
                           ")",
                           &foo1_result,
                           "faultCode", &bar_code,
                           "faultString", &bar_string,
                           "faultCode", &nosuch_code,
                           "faultString", &nosuch_string,
                           &foo2_result);
    xmlrpc_DECREF(valueP);
    TEST_NO_FAULT(&env);    
    TEST(foo1_result == 42);
    TEST(bar_code == 123);
    TEST(streq(bar_string, "Test fault"));
    TEST(nosuch_code == XMLRPC_NO_SUCH_METHOD_ERROR);
    TEST(foo2_result == 42);
    xmlrpc_DECREF(multiP);
    free(bar_string);
    free(nosuch_string);
    

    /* Now for some invalid multi calls */

    multiP = xmlrpc_build_value(&env,
                               "(({s:s,s:V}{s:s,s:()}{s:s,s:V}))",
                               "methodName", "test.foo",
                               "params", argArrayP,
                               "methodName", "system.multicall",
                               "params",
                               "methodName", "test.foo",
                               "params", argArrayP);
    TEST_NO_FAULT(&env);    
    doRpc(&env, registryP, "system.multicall", multiP, MULTI_CALLINFO,
          &valueP);
    TEST_FAULT(&env, XMLRPC_REQUEST_REFUSED_ERROR);

    xmlrpc_DECREF(multiP);
    
    multiP = xmlrpc_build_value(&env,
                                "(({s:s,s:V}d))",
                                "methodName", "test.foo",
                                "params", argArrayP,
                                5.0);

    TEST_NO_FAULT(&env);
    doRpc(&env, registryP, "system.multicall", multiP, MULTI_CALLINFO,
          &valueP);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    xmlrpc_DECREF(multiP);

    multiP = xmlrpc_build_value(&env,
                                "({s:s,s:V})",
                                "methodName", "test.foo",
                                "params", argArrayP);

    TEST_NO_FAULT(&env);
    doRpc(&env, registryP, "system.multicall", multiP, MULTI_CALLINFO,
          &valueP);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    xmlrpc_DECREF(multiP);

    multiP = xmlrpc_build_value(&env, "(({}))");
    TEST_NO_FAULT(&env);
    doRpc(&env, registryP, "system.multicall", multiP, MULTI_CALLINFO,
          &valueP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_DECREF(multiP);

    multiP = xmlrpc_build_value(&env, "(({s:s}))",
                                "methodName", "test.foo");
    TEST_NO_FAULT(&env);
    doRpc(&env, registryP, "system.multicall", multiP, MULTI_CALLINFO,
          &valueP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_DECREF(multiP);

    
    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void
testCall(xmlrpc_registry * const registryP) {

    xmlrpc_env env;
    xmlrpc_env env2;
    xmlrpc_value * argArrayP;
    xmlrpc_value * valueP;
    xmlrpc_int32 i;

    printf("  Running call tests.");

    xmlrpc_env_init(&env);

    /* Build an argument array for our calls. */
    argArrayP = xmlrpc_build_value(&env, "(ii)",
                                   (xmlrpc_int32) 25, (xmlrpc_int32) 17); 
    TEST_NO_FAULT(&env);

    /* Call test.foo and check the result. */
    doRpc(&env, registryP, "test.foo", argArrayP, FOO_CALLINFO, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);
    xmlrpc_decompose_value(&env, valueP, "i", &i);
    xmlrpc_DECREF(valueP);
    TEST_NO_FAULT(&env);
    TEST(i == 42);

    /* Call test.bar and check the result. */
    xmlrpc_env_init(&env2);
    doRpc(&env2, registryP, "test.bar", argArrayP, BAR_CALLINFO, &valueP);
    TEST(env2.fault_occurred);
    TEST(env2.fault_code == 123);
    TEST(env2.fault_string && streq(env2.fault_string, "Test fault"));
    xmlrpc_env_clean(&env2);

    /* Call a non-existant method and check the result. */
    xmlrpc_env_init(&env2);
    doRpc(&env2, registryP, "test.nosuch", argArrayP, FOO_CALLINFO, &valueP);
    TEST(valueP == NULL);
    TEST_FAULT(&env2, XMLRPC_NO_SUCH_METHOD_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void
testDefaultMethod(xmlrpc_registry * const registryP) {
    
    xmlrpc_env env;
    xmlrpc_value * argArrayP;
    xmlrpc_value * valueP;
    xmlrpc_int32 i;
 
    xmlrpc_env_init(&env);

    printf("  Running default method tests.");

    /* Build an argument array for our calls. */
    argArrayP = xmlrpc_build_value(&env, "(ii)",
                                   (xmlrpc_int32) 25, (xmlrpc_int32) 17); 

    xmlrpc_registry_set_default_method(&env, registryP, &test_default,
                                       DEFAULT_SERVERINFO);
    TEST_NO_FAULT(&env);
    doRpc(&env, registryP, "test.nosuch", argArrayP, DEFAULT_CALLINFO,
          &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);
    xmlrpc_decompose_value(&env, valueP, "i", &i);
    xmlrpc_DECREF(valueP);
    TEST_NO_FAULT(&env);
    TEST(i == 84);

    /* Now try it with old method interface */

    doRpc(&env, registryP, "test.nosuch.old", argArrayP, NULL, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);
    xmlrpc_read_int(&env, valueP, &i);
    TEST_NO_FAULT(&env);
    xmlrpc_DECREF(valueP);
    TEST(i == 84);

    /* Change the default method. */
    xmlrpc_registry_set_default_method(&env, registryP, &test_default,
                                       BAR_SERVERINFO);
    TEST_NO_FAULT(&env);

    xmlrpc_DECREF(argArrayP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



static void
test_apache_dialect(void) {

    char const expectedResp[] =
        XML_PROLOGUE
        "<methodResponse " XMLNS_APACHE ">\r\n"
        "<params>\r\n"
        "<param><value><array><data>\r\n"
            "<value><ex:i8>8</ex:i8></value>\r\n"
            "<value><ex:nil/></value>\r\n"
        "</data></array></value></param>\r\n"
        "</params>\r\n"
        "</methodResponse>\r\n";

    xmlrpc_env env;
    xmlrpc_registry * registryP;
    xmlrpc_value * argArrayP;
    xmlrpc_mem_block * callP;
    xmlrpc_mem_block * responseP;

    xmlrpc_env_init(&env);

    printf("  Running apache dialect tests.");

    registryP = xmlrpc_registry_new(&env);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_set_dialect(&env, registryP, xmlrpc_dialect_i8);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_set_dialect(&env, registryP, 100);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_registry_set_dialect(&env, registryP, xmlrpc_dialect_apache);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_add_method2(&env, registryP, "test_exttype",
                                test_exttype, NULL, NULL, FOO_SERVERINFO);
    TEST_NO_FAULT(&env);

    argArrayP = xmlrpc_array_new(&env);
    TEST_NO_FAULT(&env);

    callP = xmlrpc_mem_block_new(&env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_call(&env, callP, "test_exttype", argArrayP);
    TEST_NO_FAULT(&env);

    xmlrpc_registry_process_call2(
        &env, registryP,
        xmlrpc_mem_block_contents(callP),
        xmlrpc_mem_block_size(callP),
        NULL, &responseP);

    TEST_NO_FAULT(&env);

    TEST(XMLRPC_MEMBLOCK_SIZE(char, responseP) == strlen(expectedResp));
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, responseP),
               expectedResp,
               XMLRPC_MEMBLOCK_SIZE(char, responseP)));

    xmlrpc_DECREF(argArrayP);
    xmlrpc_mem_block_free(callP);
    xmlrpc_mem_block_free(responseP);
    xmlrpc_registry_free(registryP);

    xmlrpc_env_clean(&env);

    printf("\n");
}



void
test_method_registry(void) {

    xmlrpc_env env, env2;
    xmlrpc_value * valueP;
    xmlrpc_registry * registryP;
    xmlrpc_mem_block * responseP;

    xmlrpc_env_init(&env);

    testVersion();

    printf("Running method registry tests.");

    /* Create a new registry. */
    registryP = xmlrpc_registry_new(&env);
    TEST_NO_FAULT(&env);
    TEST(registryP != NULL);

    /* Add some test methods. */
    xmlrpc_registry_add_method(&env, registryP, NULL, "test.foo",
                               test_foo_type1, FOO_SERVERINFO);
    TEST_NO_FAULT(&env);
    xmlrpc_registry_add_method2(&env, registryP, "test.bar",
                                test_bar, NULL, barHelp, BAR_SERVERINFO);
    TEST_NO_FAULT(&env);

    printf("\n");
    testCall(registryP);

    test_system_multicall(registryP);

    xmlrpc_env_init(&env2);
    xmlrpc_registry_process_call2(&env, registryP,
                                  expat_error_data,
                                  strlen(expat_error_data),
                                  NULL,
                                  &responseP);
    TEST_NO_FAULT(&env);
    TEST(responseP != NULL);
    valueP = xmlrpc_parse_response(&env2, xmlrpc_mem_block_contents(responseP),
                                  xmlrpc_mem_block_size(responseP));
    TEST(valueP == NULL);
    TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
    xmlrpc_mem_block_free(responseP);
    xmlrpc_env_clean(&env2);

    printf("\n");

    testDefaultMethod(registryP);

    test_system_listMethods(registryP);

    test_system_methodExist(registryP);

    test_system_methodHelp(registryP);

    test_system_capabilities(registryP);

    test_system_getCapabilities(registryP);

    test_signature();

    test_disable_introspection();

    test_apache_dialect();
    
    /* Test cleanup code (w/memprof). */
    xmlrpc_registry_free(registryP);

    printf("\n");

    xmlrpc_env_clean(&env);
}
