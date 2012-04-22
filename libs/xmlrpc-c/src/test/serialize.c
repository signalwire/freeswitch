#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"

#include "test.h"
#include "xml_data.h"
#include "girstring.h"
#include "serialize_value.h"

#include "serialize.h"


static void
test_serialize_basic(void) {

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    /* Build a nice, messy value to serialize. We should attempt to use
    ** use every data type except double (which doesn't serialize in a
    ** portable manner. */
    v = xmlrpc_build_value(&env, "(iibbs68())",
                           (xmlrpc_int32) INT_MAX, (xmlrpc_int32) INT_MIN,
                           (xmlrpc_bool) 0, (xmlrpc_bool) 1,
                           "Hello, world! <&>",
                           "base64 data", (size_t) 11,
                           "19980717T14:08:55");
    TEST_NO_FAULT(&env);
    
    /* Serialize the value. */
    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_data));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_data, size) == 0);
    
    /* (Debugging code to display the value.) */
    /* XMLRPC_TYPED_MEM_BLOCK_APPEND(char, &env, output, "\0", 1);
    ** TEST_NO_FAULT(&env);
    ** printf("%s\n", XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output)); */

    /* Clean up our value. */
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_methodResponse(void) {

    /* Serialize a methodResponse. */

    char const serialized_response[] =
        XML_PROLOGUE
        "<methodResponse>\r\n"
        "<params>\r\n"
        "<param><value><i4>30</i4></value></param>\r\n"
        "</params>\r\n"
        "</methodResponse>\r\n";

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 30);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_response(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_response));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_response, size) == 0);

    /* Clean up our methodResponse. */
    xmlrpc_DECREF(v);
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_methodCall(void) {

    /* Serialize a methodCall. */

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    v = xmlrpc_build_value(&env, "(ii)", (xmlrpc_int32) 10, (xmlrpc_int32) 20);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_call(&env, output, "gloom&doom", v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_call));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_call, size) == 0);

    /* Clean up our methodCall. */
    xmlrpc_DECREF(v);
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_fault(void) {
    /* Serialize a fault. */

    xmlrpc_env env;
    xmlrpc_env fault;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_env_init(&fault);
    xmlrpc_env_set_fault(&fault, 6, "A fault occurred");
    xmlrpc_serialize_fault(&env, output, &fault);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_fault));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_fault, size) == 0);

    /* Clean up our fault. */
    xmlrpc_env_clean(&fault);
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_apache_value(void) {

    char const serializedData[] =
        "<value><array><data>\r\n"
            "<value><i4>7</i4></value>\r\n"
            "<value><ex.i8>8</ex.i8></value>\r\n"
            "<value><ex.nil/></value>\r\n"
        "</data></array></value>";

    xmlrpc_env env;
    xmlrpc_value * valueP;
    xmlrpc_mem_block * outputP;
    size_t size;

    xmlrpc_env_init(&env);

    valueP = xmlrpc_build_value(&env, "(iIn)", 7, (xmlrpc_int64)8);
    TEST_NO_FAULT(&env);
    
    outputP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value2(&env, outputP, valueP, xmlrpc_dialect_apache);
    TEST_NO_FAULT(&env);

    size = XMLRPC_MEMBLOCK_SIZE(char, outputP);

    TEST(size == strlen(serializedData));
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, outputP), serializedData, size));
    
    XMLRPC_MEMBLOCK_FREE(char, outputP);
    xmlrpc_DECREF(valueP);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_apache_params(void) {

    char const serializedData[] =
        "<params>\r\n"
            "<param><value><i4>7</i4></value></param>\r\n"
            "<param><value><ex.i8>8</ex.i8></value></param>\r\n"
        "</params>\r\n";

    xmlrpc_env env;
    xmlrpc_value * paramArrayP;
    xmlrpc_mem_block * outputP;
    size_t size;

    xmlrpc_env_init(&env);

    paramArrayP = xmlrpc_build_value(&env, "(iI)", 7, (xmlrpc_int64)8);
    TEST_NO_FAULT(&env);
    
    outputP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_params2(&env, outputP, paramArrayP,
                             xmlrpc_dialect_apache);
    TEST_NO_FAULT(&env);

    size = XMLRPC_MEMBLOCK_SIZE(char, outputP);

    TEST(size == strlen(serializedData));
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, outputP), serializedData, size));
    
    XMLRPC_MEMBLOCK_FREE(char, outputP);
    xmlrpc_DECREF(paramArrayP);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_apache_response(void) {

    char const serializedData[] =
        XML_PROLOGUE
        "<methodResponse>\r\n"
        "<params>\r\n"
        "<param><value><ex.i8>8</ex.i8></value></param>\r\n"
        "</params>\r\n"
        "</methodResponse>\r\n";

    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_mem_block * outputP;
    size_t size;

    xmlrpc_env_init(&env);

    resultP = xmlrpc_i8_new(&env, 8);
    TEST_NO_FAULT(&env);
    
    outputP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_response2(&env, outputP, resultP,
                               xmlrpc_dialect_apache);
    TEST_NO_FAULT(&env);

    size = XMLRPC_MEMBLOCK_SIZE(char, outputP);

    TEST(size == strlen(serializedData));
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, outputP), serializedData, size));
    
    XMLRPC_MEMBLOCK_FREE(char, outputP);
    xmlrpc_DECREF(resultP);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_apache(void) {

    /* Serialize various things using the Apache dialect of XML-RPC */

    test_serialize_apache_value();
    test_serialize_apache_params();
    test_serialize_apache_response();
}



void 
test_serialize(void) {

    printf("Running serialize tests.");

    test_serialize_basic();
    printf("\n");
    test_serialize_value();
    test_serialize_methodResponse();
    test_serialize_methodCall();
    test_serialize_fault();
    test_serialize_apache();

    printf("\n");
    printf("Serialize tests done.\n");
}
