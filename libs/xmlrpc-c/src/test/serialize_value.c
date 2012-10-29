#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"

#include "testtool.h"
#include "girstring.h"

#include "serialize_value.h"


static void
test_serialize_string(void) {

    /* Test serialize of a string, including all the line ending
       complexity.
    */

    xmlrpc_env env;
    xmlrpc_value * v;
    xmlrpc_mem_block * xmlP;         /* Serialized result */
    
    xmlrpc_env_init(&env);

    TEST_NO_FAULT(&env);

    v = xmlrpc_string_new(&env, "hello world");
    TEST_NO_FAULT(&env);

    xmlP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    xmlrpc_serialize_value(&env, xmlP, v);
    TEST_NO_FAULT(&env);
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
               "<value><string>hello world</string></value>",
               XMLRPC_MEMBLOCK_SIZE(char, xmlP)));
    XMLRPC_MEMBLOCK_FREE(char, xmlP);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new(&env, "");
    TEST_NO_FAULT(&env);
    xmlP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    xmlrpc_serialize_value(&env, xmlP, v);
    TEST_NO_FAULT(&env);
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
               "<value><string></string></value>",
               XMLRPC_MEMBLOCK_SIZE(char, xmlP)));
    XMLRPC_MEMBLOCK_FREE(char, xmlP);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new_lp(&env, 7, "foo\0bar");
    TEST_NO_FAULT(&env);
    xmlP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    xmlrpc_serialize_value(&env, xmlP, v);
    TEST_NO_FAULT(&env);
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
               "<value><string>foo\0bar</string></value>",
               XMLRPC_MEMBLOCK_SIZE(char, xmlP)));
    XMLRPC_MEMBLOCK_FREE(char, xmlP);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new_lp(&env, 7, "foo\nbar");
    TEST_NO_FAULT(&env);
    xmlP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    xmlrpc_serialize_value(&env, xmlP, v);
    TEST_NO_FAULT(&env);
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
               "<value><string>foo\nbar</string></value>",
               XMLRPC_MEMBLOCK_SIZE(char, xmlP)));
    XMLRPC_MEMBLOCK_FREE(char, xmlP);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new_lp(&env, 8, "foo\r\nbar");
    TEST_NO_FAULT(&env);
    xmlP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    xmlrpc_serialize_value(&env, xmlP, v);
    TEST_NO_FAULT(&env);
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
               "<value><string>foo\nbar</string></value>",
               XMLRPC_MEMBLOCK_SIZE(char, xmlP)));
    XMLRPC_MEMBLOCK_FREE(char, xmlP);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new_lp(&env, 7, "foo\rbar");
    TEST_NO_FAULT(&env);
    xmlP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    xmlrpc_serialize_value(&env, xmlP, v);
    TEST_NO_FAULT(&env);
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
               "<value><string>foo\nbar</string></value>",
               XMLRPC_MEMBLOCK_SIZE(char, xmlP)));
    XMLRPC_MEMBLOCK_FREE(char, xmlP);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new_lp_cr(&env, 7, "foo\rbar");
    TEST_NO_FAULT(&env);
    xmlP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    xmlrpc_serialize_value(&env, xmlP, v);
    TEST_NO_FAULT(&env);
    TEST(memeq(XMLRPC_MEMBLOCK_CONTENTS(char, xmlP),
               "<value><string>foo&#x0d;bar</string></value>",
               XMLRPC_MEMBLOCK_SIZE(char, xmlP)));
    XMLRPC_MEMBLOCK_FREE(char, xmlP);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
testOneDouble(double const value) {

    /* Test serialize of a double.  */

    xmlrpc_env env;
    xmlrpc_value * valueP;
    xmlrpc_mem_block * serializedP;
    char * result;
        /* serialized result, as asciiz string */
    size_t resultLength;
        /* Length in characters of the serialized result */
    double serializedValue;
    char nextChar;
    int itemsMatched;
    
    xmlrpc_env_init(&env);

    /* Build a double to serialize */
    valueP = xmlrpc_double_new(&env, value);
    TEST_NO_FAULT(&env);
    
    /* Serialize the value. */
    serializedP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_serialize_value(&env, serializedP, valueP);
    TEST_NO_FAULT(&env);

    /* Make sure we serialized the correct value.  Note that because
       doubles aren't precise, this might serialize as 3.1415899999
       or something like that.  So we check it arithmetically.
    */
    resultLength = XMLRPC_MEMBLOCK_SIZE(char, serializedP);
    result = malloc(resultLength + 1);

    memcpy(result, XMLRPC_MEMBLOCK_CONTENTS(char, serializedP), resultLength);
    result[resultLength] = '\0';
    
    itemsMatched = sscanf(result, 
                          "<value><double>%lf</double></value>\r\n%c",
                          &serializedValue, &nextChar);

    TEST(itemsMatched == 1);
    TESTFLOATEQUAL(serializedValue, value);

    free(result);
    
    /* Clean up our value. */
    XMLRPC_TYPED_MEM_BLOCK_FREE(char, serializedP);
    xmlrpc_DECREF(valueP);

    xmlrpc_env_clean(&env);
}



static void
test_serialize_double(void) {

    testOneDouble(0);
    testOneDouble(1);
    testOneDouble(0.3);
    testOneDouble(4.9);
    testOneDouble(9.9999999);
    testOneDouble(-8);
    testOneDouble(-.7);
    testOneDouble(-2.5);
    testOneDouble(3.14159);
    testOneDouble(1.2E37);
    testOneDouble(1.2E-37);
    testOneDouble(-5E200);
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



void 
test_serialize_value(void) {

    printf("  Running serialize value tests.");

    test_serialize_string();

    test_serialize_double();

    test_serialize_struct();

    printf("\n");
    printf("  Serialize value tests done.\n");
}
