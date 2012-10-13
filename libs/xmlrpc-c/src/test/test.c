/* Copyright information is at the end of the file. */

#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "casprintf.h"

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/string_int.h"

#include "bool.h"
#include "testtool.h"
#include "value.h"
#include "serialize.h"
#include "parse_xml.h"
#include "cgi.h"
#include "xml_data.h"
#include "client.h"
#include "abyss.h"
#include "server_abyss.h"
#include "method_registry.h"

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

bool const runningUnderWindows =
#ifdef WIN32
    true;
#else
    false;
#endif


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

static void
testVersion(void) {

    unsigned int major, minor, point;

    xmlrpc_version(&major, &minor, &point);

#ifndef WIN32    
    /* xmlrpc_version_major, etc. are not exported from a Windows DLL */

    TEST(major = xmlrpc_version_major);
    TEST(minor = xmlrpc_version_minor);
    TEST(point = xmlrpc_version_point);
#endif
}



static void
testEnv(void) {
    xmlrpc_env env, env2;

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
    TEST(xmlrpc_streq(env.fault_string, test_string_1));

    /* Change an existing fault. */
    xmlrpc_env_set_fault(&env, 2, test_string_2);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 2);
    TEST(xmlrpc_streq(env.fault_string, test_string_2));

    /* Set a fault with a format string. */
    xmlrpc_env_set_fault_formatted(&env, 3, "a%s%d", "bar", 9);
    TEST(env.fault_occurred);
    TEST(env.fault_code == 3);
    TEST(xmlrpc_streq(env.fault_string, "abar9"));

    /* Test cleanup code (with help from memprof). */
    xmlrpc_env_clean(&env);

    /* Test cleanup code on in absence of xmlrpc_env_set_fault. */
    xmlrpc_env_init(&env2);
    xmlrpc_env_clean(&env2);
}



static void
testMemBlock(void) {
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
    TEST(xmlrpc_streq(xmlrpc_mem_block_contents(block), test_string_1));

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



static void
testBase64Conversion(void) {

    xmlrpc_env env;
    char ** triplet;

    xmlrpc_env_init(&env);

    for (triplet = base64_triplets; *triplet != NULL; triplet += 3) {
        char * bin_data;
        char * nocrlf_ascii_data;
        char * ascii_data;
        xmlrpc_mem_block * output;

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
    {
        xmlrpc_env env2;
        xmlrpc_mem_block * output;

        xmlrpc_env_init(&env2);
        output = xmlrpc_base64_decode(&env2, "====", 4);
        TEST(output == NULL);
        TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
        xmlrpc_env_clean(&env2);
    }
    /* Now for something broken in a really sneaky way... */
    {
        xmlrpc_env env2;
        xmlrpc_mem_block * output;
        xmlrpc_env_init(&env2);
        output = xmlrpc_base64_decode(&env2, "a==", 4);
        TEST(output == NULL);
        TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
        xmlrpc_env_clean(&env2);
    }
    xmlrpc_env_clean(&env);
}



static void
testBoundsChecks(void) {

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



static void
testNestingLimit(void) {

    xmlrpc_env env;
    xmlrpc_value *val;

    xmlrpc_env_init(&env);
    
    /* Test with an adequate limit for a result value which is an
       array which contains an element which is a struct, whose values
       are simple: 3.
    */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, 3);
    val = xmlrpc_parse_response(&env,
                                good_response_xml, strlen(good_response_xml));
    TEST_NO_FAULT(&env);
    TEST(val != NULL);
    xmlrpc_DECREF(val);

    /* Test with an inadequate limit. */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, 2);
    val = xmlrpc_parse_response(&env,
                                good_response_xml, strlen(good_response_xml));
    TEST_FAULT(&env, XMLRPC_PARSE_ERROR); /* BREAKME - Will change. */
    TEST(val == NULL);

    /* Reset the default limit. */
    xmlrpc_limit_set(XMLRPC_NESTING_LIMIT_ID, XMLRPC_NESTING_LIMIT_DEFAULT);
    TEST(xmlrpc_limit_get(XMLRPC_NESTING_LIMIT_ID)
         == XMLRPC_NESTING_LIMIT_DEFAULT);

    xmlrpc_env_clean(&env);
}



static void
testXmlSizeLimit(void) {

    xmlrpc_env env;
    const char * methodName;
    xmlrpc_value * paramsP;
    
    /* NOTE - This test suite only verifies the last-ditch size-checking
       code.  There should also be matching code in all server (and
       preferably all client) modules as well.
    */

    /* Set our XML size limit to something ridiculous. */
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, 6);
    
    /* Attempt to parse a call. */
    xmlrpc_env_init(&env);
    xmlrpc_parse_call(&env, serialized_call, strlen(serialized_call),
                      &methodName, &paramsP);
    TEST_FAULT(&env, XMLRPC_LIMIT_EXCEEDED_ERROR);
    xmlrpc_env_clean(&env);

    {
        xmlrpc_value * resultP;
        int faultCode;
        const char * faultString;

        /* Attempt to parse a response. */
        xmlrpc_env_init(&env);
        xmlrpc_parse_response2(&env,
                               good_response_xml, strlen(good_response_xml),
                               &resultP, &faultCode, &faultString);
        TEST_FAULT(&env, XMLRPC_LIMIT_EXCEEDED_ERROR);
        xmlrpc_env_clean(&env);
    }
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
**  allowed-by-Xmlrpc-c encodings, etc.
*/

/* The test program is designed to be run with the 'test' source directory
   (which also contains the test program itself) as the current
   directory.  Except on Windows, where the Bin directory (which also contains
   the test program itself) is supposed to be the current directory.
*/
#define TESTDATA_DIR "data"

static const char * goodRequests[] = {
    TESTDATA_DIR DIRECTORY_SEPARATOR "req_out_of_order.xml",
    TESTDATA_DIR DIRECTORY_SEPARATOR "req_no_params.xml",
    TESTDATA_DIR DIRECTORY_SEPARATOR "req_value_name.xml",
    NULL
};

#define MAX_SAMPLE_FILE_LEN (16 * 1024)



static void
reportFileOpenError(const char * const path,
                    int          const openErrno) {

    if (runningUnderWindows) {
        char cwdname[1024];
        char * succeeded;

        succeeded = getcwd(cwdname, sizeof(cwdname));
        if (succeeded)
            fprintf(stderr, "Running in current work directory '%s'\n",
                    cwdname);
    }
    fprintf(stderr, "Could not open file '%s'.  errno=%d (%s)\n", 
            path, openErrno, strerror(openErrno));
}



static void
readFile(const char *  const path,
         const char ** const outDataP,
         size_t *      const outSizeP) {

    static char fileBuff[MAX_SAMPLE_FILE_LEN];

    FILE * fileP;
    size_t bytesRead;

    fileP = fopen(path, "r");

    if (fileP == NULL) {
        /* Since this error is fairly likely to happen, give an
           informative error message...
        */
        reportFileOpenError(path, errno);
        exit(1);
    }
    
    /* Read in one buffer full of data, and make sure that everything
       fit.  (We perform a lazy error/no-eof/zero-length-file test using
       'bytesRead'.)
    */
    bytesRead = fread(fileBuff, sizeof(char), MAX_SAMPLE_FILE_LEN, fileP);
    TEST(0 < bytesRead && bytesRead < MAX_SAMPLE_FILE_LEN);
    
    fclose(fileP);

    *outDataP = fileBuff;
    *outSizeP = bytesRead;
}



static void
testSampleFiles(void) {

    xmlrpc_env env;
    const char ** pathP;

    xmlrpc_env_init(&env);

    /* Test our good requests. */

    for (pathP = goodRequests; *pathP != NULL; ++pathP) {
        const char * const path = *pathP;

        const char * data;
        size_t dataLen;
        const char * methodName;
        xmlrpc_value * params;

        readFile(path, &data, &dataLen);

        xmlrpc_parse_call(&env, data, dataLen, &methodName, &params);

        TEST_NO_FAULT(&env);

        strfree(methodName);
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

#if HAVE_UNICODE_WCHAR

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
#endif  /* HAVE_UNICODE_WCHAR */

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

static void
test_utf8_coding(void) {

#if HAVE_UNICODE_WCHAR
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
        TEST(xmlrpc_strneq(utf8, XMLRPC_TYPED_MEM_BLOCK_CONTENTS(char, output),
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
#endif  /* HAVE_UNICODE_WCHAR */
}



static void
test_server_cgi_maybe(void) {

#ifndef WIN32

    test_server_cgi();

#endif 
}



static void
test_client_maybe(void) {

#ifndef WIN32 /* Must get Windows Curl transport working for this to work */

    test_client();

#endif 
}



int 
main(int     argc, 
     char ** argv ATTR_UNUSED) {

    int retval;

    if (argc-1 > 0) {
        fprintf(stderr, "There are no arguments.\n");
        retval = 1;
    } else {
        testVersion();
        testEnv();
        testMemBlock();
        testBase64Conversion();
        printf("\n");
        test_value();
        testBoundsChecks();
        printf("\n");
        test_serialize();
        test_parse_xml();
        test_method_registry();
        testNestingLimit();
        testXmlSizeLimit();
        testSampleFiles();
        printf("\n");
        test_server_cgi_maybe();
        test_abyss();
        test_server_abyss();

        test_utf8_coding();

        printf("\n");

        test_client_maybe();

        printf("\n");

        /* Summarize our test run. */
        printf("Ran %d tests, %d failed, %.1f%% passed\n",
               total_tests, total_failures,
               100.0 - (100.0 * total_failures) / total_tests);

        /* Print the final result. */
        if (total_failures == 0) {
            printf("OK\n");
            retval = 0;
        } else {
            retval = 1;
            printf("FAILED\n");
        }
    }
    return retval;
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
