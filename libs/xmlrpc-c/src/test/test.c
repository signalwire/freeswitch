/* Copyright information is at the end of the file. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"

#include "test.h"
#include "value.h"
#include "serialize.h"
#include "parse_xml.h"
#include "cgi.h"
#include "xml_data.h"
#include "client.h"
#include "server_abyss.h"

/*=========================================================================
**  Test Harness
**=========================================================================
**  This is a super light-weight test harness. It's vaguely inspired by
**  Kent Beck's book on eXtreme Programming (XP)--the output is succinct,
**  new tests can be coded quickly, and the whole thing runs in a few
**  second's time.
**
**  To run the tests, type './rpctest'.
**  To check for memory leaks, install RedHat's 'memprof' utility, and
**  type 'memprof rpctest'.
**
**  If you add new tests to this file, please deallocate any data
**  structures you use in the appropriate fashion. This allows us to test
**  various destructor code for memory leaks.
*/

int total_tests = 0;
int total_failures = 0;


/*=========================================================================
**  Test Data
**=========================================================================
**  Some common test data which need to be allocated at a fixed address,
**  or which are inconvenient to allocate inline.
*/

static char* test_string_1 = "foo";
static char* test_string_2 = "bar";
static int test_int_array_1[5] = {1, 2, 3, 4, 5};
static int test_int_array_2[3] = {6, 7, 8};
static int test_int_array_3[8] = {1, 2, 3, 4, 5, 6, 7, 8};

/*=========================================================================
**  Test Suites
**=========================================================================
*/

static void test_env(void)
{
    xmlrpc_env env, env2;
    char *s;

    /* Test xmlrpc_env_init. */
    xmlrpc_env_init(&env);
    TEST(!env.fault_occurred);
    TEST(env.fault_code == 0);
    TEST(env.fault_string == NULL);

    /* Test xmlrpc_set_fault. */
    xmlrpc_env_set_fault(&env, 1, test_string_1);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 1);
    TEST(env.fault_string != test_string_1);
    TEST(strcmp(env.fault_string, test_string_1) == 0);

    /* Change an existing fault. */
    xmlrpc_env_set_fault(&env, 2, test_string_2);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 2);
    TEST(strcmp(env.fault_string, test_string_2) == 0);    

    /* Set a fault with a format string. */
    xmlrpc_env_set_fault_formatted(&env, 3, "a%s%d", "bar", 9);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 3);
    TEST(strcmp(env.fault_string, "abar9") == 0);

    /* Set a fault with an oversized string. */
    s = "12345678901234567890123456789012345678901234567890";
    xmlrpc_env_set_fault_formatted(&env, 4, "%s%s%s%s%s%s", s, s, s, s, s, s);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 4);
    TEST(strlen(env.fault_string) == 255);

    /* Test cleanup code (with help from memprof). */
    xmlrpc_env_clean(&env);

    /* Test cleanup code on in absence of xmlrpc_env_set_fault. */
    xmlrpc_env_init(&env2);
    xmlrpc_env_clean(&env2);
}

static void test_mem_block (void)
{
    xmlrpc_env env;
    xmlrpc_mem_block* block;

    xmlrpc_mem_block* typed_heap_block;
    xmlrpc_mem_block typed_auto_block;
    void** typed_contents;

    xmlrpc_env_init(&env);

    /* Allocate a zero-size block. */
    block = xmlrpc_mem_block_new(&env, 0);
    TEST_NO_FAULT(&env);
    TEST(block != NULL);
    TEST(xmlrpc_mem_block_size(block) == 0);

    /* Grow the block a little bit. */
    xmlrpc_mem_block_resize(&env, block, strlen(test_string_1) + 1);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(block) == strlen(test_string_1) + 1);
    
    /* Insert a string into the block, and resize it by large amount.
    ** We want to cause a reallocation and copy of the block contents. */
    strcpy(xmlrpc_mem_block_contents(block), test_string_1);
    xmlrpc_mem_block_resize(&env, block, 10000);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_mem_block_size(block) == 10000);
    TEST(strcmp(xmlrpc_mem_block_contents(block), test_string_1) == 0);

    /* Test cleanup code (with help from memprof). */
    xmlrpc_mem_block_free(block);
    
    /* Allocate a bigger block. */
    block = xmlrpc_mem_block_new(&env, 128);
    TEST_NO_FAULT(&env);
    TEST(block != NULL);
    TEST(xmlrpc_mem_block_size(block) == 128);

    /* Test cleanup code (with help from memprof). */
    xmlrpc_mem_block_free(block);

    /* Allocate a "typed" memory block. */
    typed_heap_block = XMLRPC_TYPED_MEM_BLOCK_NEW(void*, &env, 20);
    TEST_NO_FAULT(&env);
    TEST(typed_heap_block != NULL);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, typed_heap_block) == 20);
    typed_contents = XMLRPC_TYPED_MEM_BLOCK_CONTENTS(void*, typed_heap_block);
    TEST(typed_contents != NULL);

    /* Resize a typed memory block. */
    XMLRPC_TYPED_MEM_BLOCK_RESIZE(void*, &env, typed_heap_block, 100);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, typed_heap_block) == 100);

    /* Test cleanup code (with help from memprof). */
    XMLRPC_TYPED_MEM_BLOCK_FREE(void*, typed_heap_block);

    /* Test _INIT and _CLEAN for stack-based memory blocks. */
    XMLRPC_TYPED_MEM_BLOCK_INIT(void*, &env, &typed_auto_block, 30);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(void*, &typed_auto_block) == 30);
    XMLRPC_TYPED_MEM_BLOCK_CLEAN(void*, &typed_auto_block);

    /* Test xmlrpc_mem_block_append. */
    block = XMLRPC_TYPED_MEM_BLOCK_NEW(int, &env, 5);
    TEST_NO_FAULT(&env);
    memcpy(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(int, block),
           test_int_array_1, sizeof(test_int_array_1));
    XMLRPC_TYPED_MEM_BLOCK_APPEND(int, &env, block, test_int_array_2, 3);
    TEST(XMLRPC_TYPED_MEM_BLOCK_SIZE(int, block) == 8);
    TEST(memcmp(XMLRPC_TYPED_MEM_BLOCK_CONTENTS(int, block),
                test_int_array_3, sizeof(test_int_array_3)) == 0);
    XMLRPC_TYPED_MEM_BLOCK_FREE(int, block);

    xmlrpc_env_clean(&env);
}

static char *(base64_triplets[]) = {
    "", "", "\r\n",
    "a", "YQ==", "YQ==\r\n",
    "aa", "YWE=", "YWE=\r\n",
    "aaa", "YWFh", "YWFh\r\n",
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWmFiY"
    "2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVo=",
    "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXpBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWmFiY"
    "2Rl\r\n"
    "ZmdoaWprbG1ub3BxcnN0dXZ3eHl6QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVo=\r\n",
    NULL};

static void test_base64_conversion (void)
{
    xmlrpc_env env, env2;
    char **triplet, *bin_data, *nocrlf_ascii_data, *ascii_data;
    xmlrpc_mem_block *output;

    xmlrpc_env_init(&env);

    for (triplet = base64_triplets; *triplet != NULL; triplet += 3) {
        bin_data = *triplet;
        nocrlf_ascii_data = *(triplet + 1);
        ascii_data = *(triplet + 2);

        /* Test our encoding routine. */
        output = xmlrpc_base64_encode(&env,
                                      (unsigned char*) bin_data,
                                      strlen(bin_data));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(xmlrpc_mem_block_size(output) == strlen(ascii_data));
        TEST(memcmp(xmlrpc_mem_block_contents(output), ascii_data,
                    strlen(ascii_data)) == 0);
        xmlrpc_mem_block_free(output);

        /* Test our newline-free encoding routine. */
        output =
            xmlrpc_base64_encode_without_newlines(&env,
                                                  (unsigned char*) bin_data,
                                                  strlen(bin_data));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(xmlrpc_mem_block_size(output) == strlen(nocrlf_ascii_data));
        TEST(memcmp(xmlrpc_mem_block_contents(output), nocrlf_ascii_data,
                    strlen(nocrlf_ascii_data)) == 0);
        xmlrpc_mem_block_free(output);

        /* Test our decoding routine. */
        output = xmlrpc_base64_decode(&env, ascii_data, strlen(ascii_data));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(xmlrpc_mem_block_size(output) == strlen(bin_data));
        TEST(memcmp(xmlrpc_mem_block_contents(output), bin_data,
                    strlen(bin_data)) == 0);
        xmlrpc_mem_block_free(output);
    }

    /* Now for something broken... */
    xmlrpc_env_init(&env2);
    output = xmlrpc_base64_decode(&env2, "====", 4);
    TEST(output == NULL);
    TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
    xmlrpc_env_clean(&env2);

    /* Now for something broken in a really sneaky way... */
    xmlrpc_env_init(&env2);
    output = xmlrpc_base64_decode(&env2, "a==", 4);
    TEST(output == NULL);
    TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_clean(&env);
}



static void test_bounds_checks (void)
{
    xmlrpc_env env;
    xmlrpc_value *array;
    int i1, i2, i3, i4;

    /* Get an array to work with. */
    xmlrpc_env_init(&env);
    array = xmlrpc_build_value(&env, "(iii)", 100, 200, 300);
    TEST_NO_FAULT(&env);
    xmlrpc_env_clean(&env);
    
    /* Test xmlrpc_decompose_value with too few values. */
    xmlrpc_env_init(&env);
    xmlrpc_decompose_value(&env, array, "(iiii)", &i1, &i2, &i3, &i4);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);

    /* Test xmlrpc_decompose_value with too many values. */
    xmlrpc_env_init(&env);
    xmlrpc_decompose_value(&env, array, "(ii)", &i1, &i2, &i3, &i4);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);

    /* Dispose of our array. */
    xmlrpc_DECREF(array);
}



/*=========================================================================
**  test_method_registry
**=========================================================================
**  We need to define some static callbacks to test this code.
*/

#define FOO_USER_DATA ((void*) 0xF00)
#define BAR_USER_DATA ((void*) 0xBAF)

static xmlrpc_value *test_foo (xmlrpc_env *env,
                               xmlrpc_value *param_array,
                               void *user_data)
{
    xmlrpc_int32 x, y;

    TEST_NO_FAULT(env);
    TEST(param_array != NULL);
    TEST(user_data == FOO_USER_DATA);

    xmlrpc_decompose_value(env, param_array, "(ii)", &x, &y);
    TEST_NO_FAULT(env);
    TEST(x == 25);
    TEST(y == 17);

    return xmlrpc_build_value(env, "i", (xmlrpc_int32) x + y);
}

static xmlrpc_value *test_bar (xmlrpc_env *env,
                               xmlrpc_value *param_array,
                               void *user_data)
{
    xmlrpc_int32 x, y;

    TEST_NO_FAULT(env);
    TEST(param_array != NULL);
    TEST(user_data == BAR_USER_DATA);

    xmlrpc_decompose_value(env, param_array, "(ii)", &x, &y);
    TEST_NO_FAULT(env);
    TEST(x == 25);
    TEST(y == 17);

    xmlrpc_env_set_fault(env, 123, "Test fault");
    return NULL;
}

static xmlrpc_value *
test_default(xmlrpc_env *   const env,
             const char *   const host ATTR_UNUSED,
             const char *   const method_name ATTR_UNUSED,
             xmlrpc_value * const param_array,
             void *         const user_data) {

    xmlrpc_int32 x, y;

    TEST_NO_FAULT(env);
    TEST(param_array != NULL);
    TEST(user_data == FOO_USER_DATA);

    xmlrpc_decompose_value(env, param_array, "(ii)", &x, &y);
    TEST_NO_FAULT(env);
    TEST(x == 25);
    TEST(y == 17);

    return xmlrpc_build_value(env, "i", 2 * (x + y));
}

static xmlrpc_value *
process_call_helper (xmlrpc_env *env,
                     xmlrpc_registry *registry,
                     const char *method_name,
                     xmlrpc_value *arg_array)
{
    xmlrpc_mem_block *call, *response;
    xmlrpc_value *value;

    /* Build a call, and tell the registry to handle it. */
    call = xmlrpc_mem_block_new(env, 0);
    TEST_NO_FAULT(env);
    xmlrpc_serialize_call(env, call, method_name, arg_array);
    TEST_NO_FAULT(env);
    response = xmlrpc_registry_process_call(env, registry, NULL,
                                            xmlrpc_mem_block_contents(call),
                                            xmlrpc_mem_block_size(call));
    TEST_NO_FAULT(env);
    TEST(response != NULL);

    /* Parse the response. */
    value = xmlrpc_parse_response(env, xmlrpc_mem_block_contents(response),
                                  xmlrpc_mem_block_size(response));

    xmlrpc_mem_block_free(call);
    xmlrpc_mem_block_free(response);
    return value;
}



static void
test_method_registry(void) {

    xmlrpc_env env, env2;
    xmlrpc_value *arg_array, *value;
    xmlrpc_registry *registry;
    xmlrpc_mem_block *response;
    xmlrpc_int32 i;

    xmlrpc_value *multi;
    xmlrpc_int32 foo1_result, foo2_result;
    xmlrpc_int32 bar_code, nosuch_code, multi_code, bogus1_code, bogus2_code;
    char *bar_string, *nosuch_string, *multi_string;
    char *bogus1_string, *bogus2_string;

    xmlrpc_env_init(&env);

    /* Create a new registry. */
    registry = xmlrpc_registry_new(&env);
    TEST(registry != NULL);
    TEST_NO_FAULT(&env);

    /* Add some test methods. */
    xmlrpc_registry_add_method(&env, registry, NULL, "test.foo",
                               test_foo, FOO_USER_DATA);
    TEST_NO_FAULT(&env);
    xmlrpc_registry_add_method(&env, registry, NULL, "test.bar",
                               test_bar, BAR_USER_DATA);
    TEST_NO_FAULT(&env);

    /* Build an argument array for our calls. */
    arg_array = xmlrpc_build_value(&env, "(ii)",
                                   (xmlrpc_int32) 25, (xmlrpc_int32) 17); 
    TEST_NO_FAULT(&env);

    /* Call test.foo and check the result. */
    value = process_call_helper(&env, registry, "test.foo", arg_array);
    TEST_NO_FAULT(&env);
    TEST(value != NULL);
    xmlrpc_decompose_value(&env, value, "i", &i);
    xmlrpc_DECREF(value);
    TEST_NO_FAULT(&env);
    TEST(i == 42);

    /* Call test.bar and check the result. */
    xmlrpc_env_init(&env2);
    value = process_call_helper(&env2, registry, "test.bar", arg_array);
    TEST_FAULT(&env2, 123);
    TEST(env2.fault_string && strcmp(env2.fault_string, "Test fault") == 0);
    xmlrpc_env_clean(&env2);

    /* Call a non-existant method and check the result. */
    xmlrpc_env_init(&env2);
    value = process_call_helper(&env2, registry, "test.nosuch", arg_array);
    TEST(value == NULL);
    TEST_FAULT(&env2, XMLRPC_NO_SUCH_METHOD_ERROR);
    xmlrpc_env_clean(&env2);

    /* Test system.multicall. */
    multi = xmlrpc_build_value(&env,
                               "(({s:s,s:V}{s:s,s:V}{s:s,s:V}"
                               "{s:s,s:()}s{}{s:s,s:V}))",
                               "methodName", "test.foo",
                               "params", arg_array,
                               "methodName", "test.bar",
                               "params", arg_array,
                               "methodName", "test.nosuch",
                               "params", arg_array,
                               "methodName", "system.multicall",
                               "params",
                               "bogus_entry",
                               "methodName", "test.foo",
                               "params", arg_array);
    TEST_NO_FAULT(&env);    
    value = process_call_helper(&env, registry, "system.multicall", multi);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, value,
                           "((i){s:i,s:s,*}{s:i,s:s,*}"
                           "{s:i,s:s,*}{s:i,s:s,*}{s:i,s:s,*}(i))",
                           &foo1_result,
                           "faultCode", &bar_code,
                           "faultString", &bar_string,
                           "faultCode", &nosuch_code,
                           "faultString", &nosuch_string,
                           "faultCode", &multi_code,
                           "faultString", &multi_string,
                           "faultCode", &bogus1_code,
                           "faultString", &bogus1_string,
                           "faultCode", &bogus2_code,
                           "faultString", &bogus2_string,
                           &foo2_result);
    xmlrpc_DECREF(value);
    TEST_NO_FAULT(&env);    
    TEST(foo1_result == 42);
    TEST(bar_code == 123);
    TEST(strcmp(bar_string, "Test fault") == 0);
    TEST(nosuch_code == XMLRPC_NO_SUCH_METHOD_ERROR);
    TEST(multi_code == XMLRPC_REQUEST_REFUSED_ERROR);
    TEST(foo2_result == 42);
    xmlrpc_DECREF(multi);
    free(bar_string);
    free(nosuch_string);
    free(multi_string);
    free(bogus1_string);
    free(bogus2_string);

    /* PASS bogus XML data and make sure our parser pukes gracefully.
    ** (Because of the way the code is laid out, and the presence of other
    ** test suites, this lets us skip tests for invalid XML-RPC data.) */
    xmlrpc_env_init(&env2);
    response = xmlrpc_registry_process_call(&env, registry, NULL,
                                            expat_error_data,
                                            strlen(expat_error_data));
    TEST_NO_FAULT(&env);
    TEST(response != NULL);
    value = xmlrpc_parse_response(&env2, xmlrpc_mem_block_contents(response),
                                  xmlrpc_mem_block_size(response));
    TEST(value == NULL);
    TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
    xmlrpc_mem_block_free(response);
    xmlrpc_env_clean(&env2);

    xmlrpc_registry_set_default_method(&env, registry, &test_default,
                                       FOO_USER_DATA);
    TEST_NO_FAULT(&env);
    value = process_call_helper(&env, registry, "test.nosuch", arg_array);
    TEST_NO_FAULT(&env);
    TEST(value != NULL);
    xmlrpc_decompose_value(&env, value, "i", &i);
    xmlrpc_DECREF(value);
    TEST_NO_FAULT(&env);
    TEST(i == 84);

    /* Change the default method. */
    xmlrpc_registry_set_default_method(&env, registry, &test_default,
                                       BAR_USER_DATA);
    TEST_NO_FAULT(&env);
    
    /* Test cleanup code (w/memprof). */
    xmlrpc_registry_free(registry);
    xmlrpc_DECREF(arg_array);

    xmlrpc_env_clean(&env);
}

static void test_nesting_limit (void)
{
    xmlrpc_env env;
    xmlrpc_value *val;

    xmlrpc_env_init(&env);
    
    /* Test with an adequate limit for (...(...()...)...). */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, 2);
    val = xmlrpc_parse_response(&env, correct_value, strlen(correct_value));
    TEST_NO_FAULT(&env);
    TEST(val != NULL);
    xmlrpc_DECREF(val);

    /* Test with an inadequate limit. */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, 1);
    val = xmlrpc_parse_response(&env, correct_value, strlen(correct_value));
    TEST_FAULT(&env, XMLRPC_PARSE_ERROR); /* BREAKME - Will change. */
    TEST(val == NULL);

    /* Reset the default limit. */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, XMLRPC_NESTING_LIMIT_DEFAULT);
    TEST(xmlrpc_limit_get(XMLRPC_NESTING_LIMIT_ID)
         == XMLRPC_NESTING_LIMIT_DEFAULT);

    xmlrpc_env_clean(&env);
}

static void test_xml_size_limit (void)
{
    xmlrpc_env env;
    const char *method_name;
    xmlrpc_value *params, *val;
    

    /* NOTE - This test suite only verifies the last-ditch size-checking
    ** code.  There should also be matching code in all server (and
    ** preferably all client) modules as well. */

    /* Set our XML size limit to something ridiculous. */
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, 6);
    
    /* Attempt to parse a call. */
    xmlrpc_env_init(&env);
    xmlrpc_parse_call(&env, serialized_call, strlen(serialized_call),
                      &method_name, &params);
    TEST_FAULT(&env, XMLRPC_LIMIT_EXCEEDED_ERROR);
    TEST(method_name == NULL);
    TEST(params == NULL);
    xmlrpc_env_clean(&env);

    /* Attempt to parse a response. */
    xmlrpc_env_init(&env);
    val = xmlrpc_parse_response(&env, correct_value, strlen(correct_value));
    TEST_FAULT(&env, XMLRPC_LIMIT_EXCEEDED_ERROR);
    TEST(val == NULL);
    xmlrpc_env_clean(&env);

    /* Reset the default limit. */
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, XMLRPC_XML_SIZE_LIMIT_DEFAULT);
}

/*=========================================================================
**  test_sample_files
**=========================================================================
**  Read in a bunch of sample test files and make sure we get plausible
**  results.
**
**  We use these files to test strange-but-legal encodings, illegal-but-
**  supported encodings, etc.
*/

static char *good_requests[] = {
    "req_out_of_order.xml",
    "req_no_params.xml",
    "req_value_name.xml",
    NULL
};

#define MAX_SAMPLE_FILE_LEN (16 * 1024)

static char file_buff [MAX_SAMPLE_FILE_LEN];

static void
read_file (char *path, char **out_data, size_t *out_size)
{
    FILE *f;
    size_t bytes_read;

    /* Open the file. */
    f = fopen(path, "r");
    if (f == NULL) {
        /* Since this error is fairly likely to happen, give an
        ** informative error message... */
        fflush(stdout);
        fprintf(stderr, "Could not open file '%s'.  errno=%d (%s)\n", 
                path, errno, strerror(errno));
        abort();
    }
    
    /* Read in one buffer full of data, and make sure that everything
    ** fit.  (We perform a lazy error/no-eof/zero-length-file test using
    ** bytes_read.) */
    bytes_read = fread(file_buff, sizeof(char), MAX_SAMPLE_FILE_LEN, f);
    TEST(0 < bytes_read && bytes_read < MAX_SAMPLE_FILE_LEN);

    /* Close the file and return our data. */
    fclose(f);
    *out_data = file_buff;
    *out_size = bytes_read;
}

static void test_sample_files (void)
{
    xmlrpc_env env;
    char **paths, *path;
    char *data;
    size_t data_len;
    const char *method_name;
    xmlrpc_value *params;

    xmlrpc_env_init(&env);

    /* Test our good requests. */
    for (paths = good_requests; *paths != NULL; paths++) {
        path = *paths;
        read_file(path, &data, &data_len);
        xmlrpc_parse_call(&env, data, data_len, &method_name, &params);
        TEST_NO_FAULT(&env);
        strfree(method_name);
        xmlrpc_DECREF(params);
    }

    xmlrpc_env_clean(&env);
}


/*=========================================================================
**  test_utf8_coding
**=========================================================================
**  We need to test our UTF-8 decoder thoroughly.  Most of these test
**  cases are taken from the UTF-8-test.txt file by Markus Kuhn
**  <mkuhn@acm.org>:
**      http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
*/

#ifdef HAVE_UNICODE_WCHAR

typedef struct {
    char *utf8;
    wchar_t wcs[16];
} utf8_and_wcs;

static utf8_and_wcs good_utf8[] = {

    /* Greek 'kosme'. */
    {"\316\272\341\275\271\317\203\316\274\316\265",
     {0x03BA, 0x1F79, 0x03C3, 0x03BC, 0x03B5, 0}},

    /* First sequences of a given length. */
    /* '\000' is not a legal C string. */
    {"\302\200", {0x0080, 0}},
    {"\340\240\200", {0x0800, 0}},

    /* Last sequences of a given length. */
    {"\177", {0x007F, 0}},
    {"\337\277", {0x07FF, 0}},
    /* 0xFFFF is not a legal Unicode character. */

    /* Other boundry conditions. */
    {"\001", {0x0001, 0}},
    {"\355\237\277", {0xD7FF, 0}},
    {"\356\200\200", {0xE000, 0}},
    {"\357\277\275", {0xFFFD, 0}},

    /* Other random test cases. */
    {"", {0}},
    {"abc", {0x0061, 0x0062, 0x0063, 0}},
    {"[\302\251]", {0x005B, 0x00A9, 0x005D, 0}},
    
    {NULL, {0}}
};

static char *(bad_utf8[]) = {

    /* Continuation bytes. */
    "\200", "\277",

    /* Lonely start characters. */
    "\300", "\300x", "\300xx",
    "\340", "\340x", "\340xx", "\340xxx",

    /* Last byte missing. */
    "\340\200", "\340\200x", "\340\200xx",
    "\357\277", "\357\277x", "\357\277xx",

    /* Illegal bytes. */
    "\376", "\377",

    /* Overlong '/'. */
    "\300\257", "\340\200\257",

    /* Overlong ASCII NUL. */
    "\300\200", "\340\200\200",

    /* Maximum overlong sequences. */
    "\301\277", "\340\237\277",

    /* Illegal code positions. */
    "\357\277\276", /* U+FFFE */
    "\357\277\277", /* U+FFFF */

    /* UTF-16 surrogates (unpaired and paired). */
    "\355\240\200",
    "\355\277\277",
    "\355\240\200\355\260\200",
    "\355\257\277\355\277\277",

    /* Valid UCS-4 characters (we don't handle these yet).
    ** On systems with UCS-4 or UTF-16 wchar_t values, we
    ** may eventually handle these in some fashion. */
    "\360\220\200\200",
    "\370\210\200\200\200",
    "\374\204\200\200\200\200",

    NULL
};

/* This routine is missing on certain platforms.  This implementation
** *appears* to be correct. */
#if 0
#ifndef HAVE_WCSNCMP
int wcsncmp(wchar_t *wcs1, wchar_t* wcs2, size_t len)
{
    size_t i;
    /* XXX - 'unsigned long' should be 'uwchar_t'. */
    unsigned long c1, c2;
    for (i=0; i < len; i++) {
        c1 = wcs1[i];
        c2 = wcs2[i];
        /* This clever comparison borrowed from the GNU C Library. */
        if (c1 == 0 || c1 != c2)
            return c1 - c2;
    }
    return 0;
}
#endif /* HAVE_WCSNCMP */
#endif

static void test_utf8_coding (void)
{
    xmlrpc_env env, env2;
    utf8_and_wcs *good_data;
    char **bad_data;
    char *utf8;
    wchar_t *wcs;
    xmlrpc_mem_block *output;

    xmlrpc_env_init(&env);

    /* Test each of our valid UTF-8 sequences. */
    for (good_data = good_utf8; good_data->utf8 != NULL; good_data++) {
        utf8 = good_data->utf8;
        wcs = good_data->wcs;

        /* Attempt to validate the UTF-8 string. */
        xmlrpc_validate_utf8(&env, utf8, strlen(utf8));
        TEST_NO_FAULT(&env);

        /* Attempt to decode the UTF-8 string. */
        output = xmlrpc_utf8_to_wcs(&env, utf8, strlen(utf8));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(wcslen(wcs) == XMLRPC_TYPED_MEM_BLOCK_SIZE(wchar_t, output));
        TEST(0 ==
             wcsncmp(wcs, XMLRPC_TYPED_MEM_BLOCK_CONTENTS(wchar_t, output),
                     wcslen(wcs)));
        xmlrpc_mem_block_free(output);

        /* Test the UTF-8 encoder, too. */
        output = xmlrpc_wcs_to_utf8(&env, wcs, wcslen(wcs));
        TEST_NO_FAULT(&env);
        TEST(output != NULL);
        TEST(strlen(utf8) == XMLRPC_TYPED_MEM_BLOCK_SIZE(char, output));
        TEST(0 ==
             strncmp(utf8, XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
                     strlen(utf8)));
        xmlrpc_mem_block_free(output);
    }

    /* Test each of our illegal UTF-8 sequences. */
    for (bad_data = bad_utf8; *bad_data != NULL; bad_data++) {
        utf8 = *bad_data;
    
        /* Attempt to validate the UTF-8 string. */
        xmlrpc_env_init(&env2);
        xmlrpc_validate_utf8(&env2, utf8, strlen(utf8));
        TEST_FAULT(&env2, XMLRPC_INVALID_UTF8_ERROR);
        /* printf("Fault: %s\n", env2.fault_string); --Hand-checked */
        xmlrpc_env_clean(&env2);

        /* Attempt to decode the UTF-8 string. */
        xmlrpc_env_init(&env2);
        output = xmlrpc_utf8_to_wcs(&env2, utf8, strlen(utf8));
        TEST_FAULT(&env2, XMLRPC_INVALID_UTF8_ERROR);
        TEST(output == NULL);
        xmlrpc_env_clean(&env2);
    }

    xmlrpc_env_clean(&env);
}

#endif /* HAVE_UNICODE_WCHAR */


/*=========================================================================
**  Test Driver
**=========================================================================
*/

int 
main(int     argc, 
     char ** argv ATTR_UNUSED) {

    if (argc-1 > 0) {
        fprintf(stderr, "There are no arguments.");
        exit(1);
    }

    /* Add your test suites here. */
    test_env();
    test_mem_block();
    test_base64_conversion();
    printf("\n");
    test_value();
    test_bounds_checks();
    printf("\n");
    test_serialize();
    test_parse_xml();

    test_method_registry();
    test_nesting_limit();
    test_xml_size_limit();
    test_sample_files();
    printf("\n");

#ifndef WIN32 /* CGI unsupported in Windows */
    test_server_cgi();
#endif 
    test_server_abyss();

#ifdef HAVE_UNICODE_WCHAR
    test_utf8_coding();
#endif /* HAVE_UNICODE_WCHAR */

    printf("\n");

#ifndef WIN32 /* TODO: Client test uses curl... */
    test_client();
#endif 

    /* Summarize our test run. */
    printf("\nRan %d tests, %d failed, %.1f%% passed\n",
           total_tests, total_failures,
           100.0 - (100.0 * total_failures) / total_tests);

    /* Print the final result. */
    if (total_failures == 0) {
        printf("OK\n");
        return 0;
    }

    printf("FAILED\n");
    return 1;
}




/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */
