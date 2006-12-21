#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"

#include "test.h"
#include "xml_data.h"
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
test_serialize_double(void) {

    /* Test serialize of a double.  */

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    char * result;
        /* serialized result, as asciiz string */
    size_t resultLength;
        /* Length in characters of the serialized result */
    float serializedValue;
    char nextChar;
    int itemsMatched;
    
    xmlrpc_env_init(&env);

    /* Build a double to serialize */
    v = xmlrpc_build_value(&env, "d", 3.14159);
    TEST_NO_FAULT(&env);
    
    /* Serialize the value. */
    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value.  Note that because
       doubles aren't precise, this might serialize as 3.1415899999
       or something like that.  So we check it arithmetically.
    */
    resultLength = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    result = malloc(resultLength + 1);

    memcpy(result, XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output), 
           resultLength);
    result[resultLength] = '\0';
    
    itemsMatched = sscanf(result, 
                          "<value><double>%f</double></value>\r\n%c",
                          &serializedValue, &nextChar);

    TEST(itemsMatched == 1);
    TEST(serializedValue - 3.14159 < .000001);
    /* We'd like to test more precision, but sscanf doesn't do doubles */

    free(result);
    
    /* Clean up our value. */
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_struct(void) {

    /* Serialize a simple struct. */

    char const serialized_struct[] = 
        "<value><struct>\r\n" \
        "<member><name>&lt;&amp;&gt;</name>\r\n" \
        "<value><i4>10</i4></value></member>\r\n" \
        "</struct></value>";
    
    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block *output;
    size_t size;
    
    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "{s:i}", "<&>", (xmlrpc_int32) 10);
    TEST_NO_FAULT(&env);
    output = XMLRPC_TYPED_MEM_BLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value(&env, output, v);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value. */
    size = XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output);
    TEST(size == strlen(serialized_struct));
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                serialized_struct, size) == 0);
    
    /* Clean up our struct. */
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, output);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_methodResponse(void) {

    /* Serialize a methodResponse. */

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



void 
test_serialize(void) {

    printf("Running serialize tests.");

    test_serialize_basic();
    test_serialize_double();
    test_serialize_struct();
    test_serialize_methodResponse();
    test_serialize_methodCall();
    test_serialize_fault();

    printf("\n");
    printf("Serialize tests done.\n");
}
