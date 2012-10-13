/* Copyright information is at the end of the file. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "casprintf.h"
#include "girstring.h"

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/string_int.h"

#include "testtool.h"
#include "value_datetime.h"

#include "value.h"


#if HAVE_UNICODE_WCHAR

static bool
wcsneq(const wchar_t * const comparand,
       const wchar_t * const comparator,
       size_t          const length) {

    return(wcsncmp(comparand, comparator, length) == 0);
}

#endif /* HAVE_UNICODE_WCHAR */

static void
test_value_alloc_dealloc(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    /* Test allocation and deallocation (w/memprof). */
    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 5);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    xmlrpc_INCREF(v);
    xmlrpc_DECREF(v);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}


static void
test_value_int(void) { 

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_int32 i;

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_INT), "INT"));

    v = xmlrpc_int_new(&env, (xmlrpc_int32) 25);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_INT);
    xmlrpc_read_int(&env, v, &i);
    TEST_NO_FAULT(&env);
    TEST(i == 25);
    xmlrpc_DECREF(v);

    v = xmlrpc_int_new(&env, (xmlrpc_int32) -25);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_INT);
    xmlrpc_read_int(&env, v, &i);
    TEST_NO_FAULT(&env);
    TEST(i == -25);
    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 10);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_INT);
    xmlrpc_decompose_value(&env, v, "i", &i);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(i == 10);

    xmlrpc_env_clean(&env);
}



static void
test_value_bool(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_bool b;

    /* Test booleans. */

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_BOOL), "BOOL"));

    v = xmlrpc_bool_new(&env, (xmlrpc_bool) 1);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_BOOL == xmlrpc_value_type(v));
    xmlrpc_read_bool(&env, v, &b);
    TEST_NO_FAULT(&env);
    TEST(b);
    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "b", (xmlrpc_bool) 0);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_BOOL == xmlrpc_value_type(v));
    xmlrpc_decompose_value(&env, v, "b", &b);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(!b);

    xmlrpc_env_clean(&env);
}



static void
test_value_double(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    double d;

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_DOUBLE), "DOUBLE"));

    v = xmlrpc_double_new(&env, -3.25);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DOUBLE == xmlrpc_value_type(v));
    xmlrpc_read_double(&env, v, &d);
    TEST_NO_FAULT(&env);
    TEST(d == -3.25);
    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "d", 1.0);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_DOUBLE == xmlrpc_value_type(v));
    xmlrpc_decompose_value(&env, v, "d", &d);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(d == 1.0);

    xmlrpc_env_clean(&env);
}



static xmlrpc_value *
test_string_new_va(xmlrpc_env * const envP,
                   const char * const format,
                   ...) {

    va_list args;

    xmlrpc_value * v;

    va_start(args, format);

    v = xmlrpc_string_new_va(envP, format, args);

    va_end(args);

    return v;
}



static void
test_value_string_no_null(void) {

    /* Test strings (without '\0' bytes). */

    xmlrpc_value * v;
    xmlrpc_env env;
    const char * str;
    size_t len;

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_STRING), "STRING"));

    v = xmlrpc_string_new(&env, "foo");
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_STRING);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new_f(&env, "String %s, number %d", "xyz", 7);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_STRING);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "String xyz, number 7"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = test_string_new_va(&env, "String %s, number %d", "xyz", 7);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_STRING);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "String xyz, number 7"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_build_value(&env, "s", "foo");

    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));

    xmlrpc_decompose_value(&env, v, "s", &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo"));
    strfree(str);

    xmlrpc_decompose_value(&env, v, "s#", &str, &len);
    TEST_NO_FAULT(&env);
    TEST(len == strlen("foo"));
    TEST(xmlrpc_streq(str, "foo"));
    TEST(strlen(str) == strlen("foo"));
    strfree(str);

    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_string_null(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_env env2;
    const char * str;
    size_t len;

    /* Test a string with a '\0' byte. */

    xmlrpc_env_init(&env);

    v = xmlrpc_string_new_lp(&env, 7, "foo\0bar");
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));
    xmlrpc_read_string_lp(&env, v, &len, &str);
    TEST_NO_FAULT(&env);
    TEST(len == 7);
    TEST(memeq(str, "foo\0bar", 7));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_build_value(&env, "s#", "foo\0bar", (size_t) 7);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));

    xmlrpc_decompose_value(&env, v, "s#", &str, &len);
    TEST_NO_FAULT(&env);
    TEST(memeq(str, "foo\0bar", 7));
    TEST(len == 7);
    strfree(str);

    /* Test for type error when decoding a string with a zero byte to a
    ** regular C string. */
    xmlrpc_env_init(&env2);
    xmlrpc_decompose_value(&env2, v, "s", &str);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_string_multiline(void) {

    xmlrpc_env env;
    xmlrpc_value * v;
    const char * str;
    size_t len;

    xmlrpc_env_init(&env);

    /* LF line ending */

    v = xmlrpc_string_new(&env, "foo\n");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\n"));
    strfree(str);
    xmlrpc_read_string_crlf(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\r\n"));
    strfree(str);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new(&env, "foo\n\n");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\n\n"));
    strfree(str);
    xmlrpc_read_string_crlf(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\r\n\r\n"));
    strfree(str);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new(&env, "foo\nbar");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar"));
    strfree(str);
    xmlrpc_read_string_crlf(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\r\nbar"));
    strfree(str);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new(&env, "foo\nbar\n");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar\n"));
    strfree(str);
    xmlrpc_read_string_crlf(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\r\nbar\r\n"));
    strfree(str);
    xmlrpc_DECREF(v);

    v = xmlrpc_string_new(&env, "foo\nbar\nbaz");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar\nbaz"));
    strfree(str);
    xmlrpc_read_string_crlf(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\r\nbar\r\nbaz"));
    strfree(str);
    xmlrpc_DECREF(v);

    /* CR line ending */

    v = xmlrpc_string_new(&env, "foo\r");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\n"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\r\r");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\n\n"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\rbar");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\rbar\r");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar\n"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\rbar\rbaz");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar\nbaz"));
    xmlrpc_DECREF(v);
    strfree(str);

    /* CRLF line ending */

    v = xmlrpc_string_new(&env, "foo\r\n");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\n"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\r\n\r\n");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\n\n"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\r\nbar");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\r\nbar\r\n");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar\n"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new(&env, "foo\r\nbar\r\nbaz");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\nbar\nbaz"));
    xmlrpc_DECREF(v);
    strfree(str);

    /* Embedded null */

    v = xmlrpc_string_new_lp(&env, 14, "foo\r\n\0bar\r\nbaz");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string_lp(&env, v, &len, &str);
    TEST_NO_FAULT(&env);
    TEST(len == 12);
    TEST(memeq(str, "foo\n\0bar\nbaz", len));
    strfree(str);
    xmlrpc_read_string_lp_crlf(&env, v, &len, &str);
    TEST_NO_FAULT(&env);
    TEST(len == 14);
    TEST(memeq(str, "foo\r\n\0bar\r\nbaz", len));
    strfree(str);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_string_cr(void) {

    xmlrpc_env env;
    xmlrpc_value * v;
    const char * str;
    size_t len;

    xmlrpc_env_init(&env);
    v = xmlrpc_string_new_cr(&env, "foo\r\nbar\r\nbaz");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(streq(str, "foo\r\nbar\r\nbaz"));
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_string_new_lp_cr(&env, 7, "\0foo\rbar");
    TEST_NO_FAULT(&env);
    xmlrpc_read_string_lp(&env, v, &len, &str);
    TEST_NO_FAULT(&env);
    TEST(len == 7);
    TEST(memeq(str, "\0foo\rbar", len));
    xmlrpc_DECREF(v);
    strfree(str);

    xmlrpc_env_clean(&env);
}


#if HAVE_UNICODE_WCHAR

/* Here is a 3-character, NUL-terminated string, once in UTF-8 chars,
   and once in UTF-16 wchar_ts.  Note that 2 of the UTF-16 characters
   translate directly to UTF-8 bytes because only the lower 7 bits of
   each is nonzero, but the middle UTF-16 character translates to two
   UTF-8 bytes.  
*/
static char utf8_data[] = "[\xC2\xA9]";
static wchar_t wcs_data[] = {'[', 0x00A9, ']', 0x0000};

static void
test_value_string_wide_build(void) {

    xmlrpc_env env;
    xmlrpc_value * valueP;
    const wchar_t * wcs;
    size_t len;

    xmlrpc_env_init(&env);

    /* Build with build_value w# */
    valueP = xmlrpc_build_value(&env, "w#", wcs_data, (size_t)3);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    /* Verify it */
    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_DECREF(valueP);

    /* Build with build_value w */
    valueP = xmlrpc_build_value(&env, "w", wcs_data);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    /* Verify it */
    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_DECREF(valueP);
}
#endif /* HAVE_UNICODE_WCHAR */


static void
test_value_string_wide_line(void) {
        /* Test with various line delimiters */

#if HAVE_UNICODE_WCHAR
    xmlrpc_env env;
    xmlrpc_value * valueP;
    const wchar_t * wcs;
    size_t len;

    wchar_t const wcs_lines[] = {'\n', '\r', '\r', '\n', '\0'};
    wchar_t const wcs_lines_lf[] = {'\n', '\n', '\n'};
    wchar_t const wcs_lines_crlf[] = {
        '\r', '\n', '\r', '\n', '\r', '\n' };
        
    xmlrpc_env_init(&env);

    valueP = xmlrpc_string_w_new(&env, wcs_lines);
    TEST_NO_FAULT(&env);

    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_lines_lf, len));
    free((void*)wcs);

    xmlrpc_read_string_w_lp_crlf(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(len == 6);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_lines_crlf, len));
    free((void*)wcs);

    xmlrpc_DECREF(valueP);

    valueP = xmlrpc_string_w_new_cr(&env, wcs_lines);
    TEST_NO_FAULT(&env);

    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(len == 4);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_lines, len));
    free((void*)wcs);

    xmlrpc_DECREF(valueP);

    xmlrpc_env_clean(&env);
#endif /* HAVE_UNICODE_WCHAR */
}



static void 
test_value_string_wide(void) {

#if HAVE_UNICODE_WCHAR
    xmlrpc_env env;
    xmlrpc_value * valueP;
    const wchar_t * wcs;
    size_t len;

    xmlrpc_env_init(&env);

    /* Build a string from wchar_t data. */
    valueP = xmlrpc_string_w_new_lp(&env, 3, wcs_data);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    /* Extract it as a wchar_t string. */
    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_read_string_w(&env, valueP, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(wcs[3] == '\0');
    TEST(wcsneq(wcs, wcs_data, 3));
    free((void*)wcs);

    xmlrpc_decompose_value(&env, valueP, "w#", &wcs, &len);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_data, len));
    free((void*)wcs);

    {
        /* Extract it as a UTF-8 string. */
        const char * str;
        size_t len;

        xmlrpc_read_string_lp(&env, valueP, &len, &str);
        TEST_NO_FAULT(&env);
        TEST(str != NULL);
        TEST(len == 4);
        TEST(str[len] == '\0');
        TEST(xmlrpc_strneq(str, utf8_data, len));
        free((void*)str);
    }

    xmlrpc_DECREF(valueP);

    /* Build from null-terminated wchar_t string */
    valueP = xmlrpc_string_w_new(&env, wcs_data);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    /* Verify it */
    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_DECREF(valueP);

    test_value_string_wide_build();

    /* Build a string from UTF-8 data. */
    valueP = xmlrpc_string_new(&env, utf8_data);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    /* Extract it as a wchar_t string. */
    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == 0x0000);
    TEST(wcsneq(wcs, wcs_data, len));
    free((void*)wcs);
    xmlrpc_DECREF(valueP);

    /* Test with embedded NUL.  We use a length of 4 so that the terminating
       NUL actually becomes part of the string.
    */
    valueP = xmlrpc_string_w_new_lp(&env, 4, wcs_data);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    /* Extract it as a wchar_t string. */
    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 4);
    TEST(wcs[len] == '\0');
    TEST(wcsneq(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_read_string_w(&env, valueP, &wcs);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    xmlrpc_DECREF(valueP);

    test_value_string_wide_line();

    xmlrpc_env_clean(&env);
#endif /* HAVE_UNICODE_WCHAR */
}



static void
test_value_base64(void) {

    /* Test <base64> data. */

    unsigned char const data1[5] = {'a', '\0', 'b', '\n', 'c'};
    unsigned char const data2[3] = {'a', '\0', 'b'};

    xmlrpc_value * v;
    xmlrpc_env env;
    const unsigned char * data;
    size_t len;

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_BASE64), "BASE64"));

    v = xmlrpc_base64_new(&env, sizeof(data1), data1);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_BASE64 == xmlrpc_value_type(v));
    xmlrpc_read_base64(&env, v, &len, &data);
    TEST_NO_FAULT(&env);
    TEST(memeq(data, data1, sizeof(data1)));
    TEST(len == sizeof(data1));
    xmlrpc_DECREF(v);
    free((void*)data);

    v = xmlrpc_build_value(&env, "6", data2, sizeof(data2));
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_BASE64 == xmlrpc_value_type(v));
    xmlrpc_decompose_value(&env, v, "6", &data, &len);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(len == sizeof(data2));
    TEST(memeq(data, data1, sizeof(data2)));
    free((void *)data);

    xmlrpc_env_clean(&env);
}



static void
test_value_value(void) {

    xmlrpc_value *v, *v2, *v3;
    xmlrpc_env env;

    /* Test 'V' with building and parsing. */

    xmlrpc_env_init(&env);

    v2 = xmlrpc_int_new(&env, (xmlrpc_int32) 5);
    TEST_NO_FAULT(&env);
    v = xmlrpc_build_value(&env, "V", v2);
    TEST_NO_FAULT(&env);
    TEST(v == v2);
    xmlrpc_decompose_value(&env, v2, "V", &v3);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(v2 == v3);
    xmlrpc_DECREF(v3);
    xmlrpc_DECREF(v2);

    xmlrpc_env_clean(&env);
}



static void
test_value_array(void) {

    xmlrpc_value *v;
    xmlrpc_env env;
    size_t len;
    xmlrpc_value * itemP;

    /* Basic array-building test. */

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_ARRAY), "ARRAY"));

    v = xmlrpc_array_new(&env);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(v));
    len = xmlrpc_array_size(&env, v);
    TEST_NO_FAULT(&env);
    TEST(len == 0);

    itemP = xmlrpc_int_new(&env, 7);
    TEST_NO_FAULT(&env);
    xmlrpc_array_append_item(&env, v, itemP);
    TEST_NO_FAULT(&env);
    len = xmlrpc_array_size(&env, v);
    TEST_NO_FAULT(&env);
    TEST(len == 1);
    xmlrpc_DECREF(itemP);

    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "()");
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(v));
    len = xmlrpc_array_size(&env, v);
    TEST_NO_FAULT(&env);
    TEST(len == 0);
    xmlrpc_DECREF(v);

    xmlrpc_env_clean(&env);
}



static void
test_value_AS(void) {

    xmlrpc_value *v;
    xmlrpc_value *v2;
    xmlrpc_value *v3;
    xmlrpc_env env;
    size_t len;

    /* Test parsing of 'A' and 'S'. */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "((){})");
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, v, "(AS)", &v2, &v3);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(v2));
    TEST(XMLRPC_TYPE_STRUCT == xmlrpc_value_type(v3));
    len = xmlrpc_array_size(&env, v2);
    TEST_NO_FAULT(&env);
    TEST(len == 0);
    len = xmlrpc_struct_size(&env, v3);
    TEST_NO_FAULT(&env);
    TEST(len == 0);
    xmlrpc_DECREF(v2);
    xmlrpc_DECREF(v3);

    xmlrpc_env_clean(&env);
}



static void
test_value_AS_typecheck(void) {

    xmlrpc_env env;
    xmlrpc_env env2;
    xmlrpc_value *v;
    xmlrpc_value *v2;

    /* Test typechecks for 'A' and 'S'. */

    xmlrpc_env_init(&env);
    xmlrpc_env_init(&env2);

    v = xmlrpc_build_value(&env, "s", "foo");
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env2, v, "A", &v2);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);

    xmlrpc_decompose_value(&env2, v, "S", &v2);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_DECREF(v);
    xmlrpc_env_clean(&env2);
    xmlrpc_env_clean(&env);
}



static void
test_value_array2(void) {

    xmlrpc_value * arrayP;
    xmlrpc_env env;
    xmlrpc_int32 i, i1, i2, i3, i4, i5;
    xmlrpc_value * itemP;
    xmlrpc_value * subarrayP;
    size_t len;

    /* A more complex array. */

    xmlrpc_env_init(&env);

    arrayP = xmlrpc_build_value(&env, "(i(ii)i)",
                                (xmlrpc_int32) 10, (xmlrpc_int32) 20,
                                (xmlrpc_int32) 30, (xmlrpc_int32) 40);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(arrayP));

    len = xmlrpc_array_size(&env, arrayP);
    TEST_NO_FAULT(&env);
    TEST(len == 3);

    xmlrpc_array_read_item(&env, arrayP, 1, &subarrayP);
    TEST_NO_FAULT(&env);

    len = xmlrpc_array_size(&env, subarrayP);
    TEST_NO_FAULT(&env);
    TEST(len == 2);

    xmlrpc_array_read_item(&env, subarrayP, 0, &itemP);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, itemP, "i", &i);
    xmlrpc_DECREF(itemP);
    TEST_NO_FAULT(&env);
    TEST(i == 20);

    xmlrpc_DECREF(subarrayP);

    itemP = xmlrpc_array_get_item(&env, arrayP, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, itemP, "i", &i);
    TEST_NO_FAULT(&env);
    TEST(i == 10);

    xmlrpc_decompose_value(&env, arrayP, "(i(ii)i)", &i1, &i2, &i3, &i4);
    TEST_NO_FAULT(&env);
    TEST(i1 == 10 && i2 == 20 && i3 == 30 && i4 == 40);

    xmlrpc_decompose_value(&env, arrayP, "(i(i*)i)", &i1, &i2, &i3);
    TEST_NO_FAULT(&env);
    TEST(i1 == 10 && i2 == 20 && i3 == 40);

    xmlrpc_decompose_value(&env, arrayP, "(i(ii*)i)", &i1, &i2, &i3, &i4);
    TEST_NO_FAULT(&env);

    xmlrpc_decompose_value(&env, arrayP, "(i(iii)i)", &i1, &i2, &i3, &i4, &i5);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_decompose_value(&env, arrayP, "(i(i)i)", &i1, &i2, &i3, &i4, &i5);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_decompose_value(&env, arrayP, "(i(ii)i*i)",
                           &i1, &i2, &i3, &i4, &i5);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_decompose_value(&env, arrayP, "(i(iiQ)i*i)",
                           &i1, &i2, &i3, &i4, &i5);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_decompose_value(&env, arrayP, "(",
                           &i1, &i2, &i3, &i4, &i5);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_decompose_value(&env, arrayP, "(i",
                           &i1, &i2, &i3, &i4, &i5);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_decompose_value(&env, arrayP, "(i*",
                           &i1, &i2, &i3, &i4, &i5);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);


    /* Test bounds check on xmlrpc_array_get_item. */
    xmlrpc_array_read_item(&env, arrayP, 3, &itemP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_array_get_item(&env, arrayP, 3);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_array_get_item(&env, arrayP, -1);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_DECREF(arrayP);

    xmlrpc_env_clean(&env);
}



static void
test_value_array_nil(void) {

    xmlrpc_value * arrayP;
    xmlrpc_env env;
    xmlrpc_int32 i1, i2;
    xmlrpc_value * itemP;
    size_t len;

    xmlrpc_env_init(&env);

    arrayP = xmlrpc_build_value(&env, "(nini)",
                                (xmlrpc_int32) 10, (xmlrpc_int32) 20);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(arrayP));

    len = xmlrpc_array_size(&env, arrayP);
    TEST_NO_FAULT(&env);
    TEST(len == 4);

    itemP = xmlrpc_array_get_item(&env, arrayP, 0);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, itemP, "n");
    TEST_NO_FAULT(&env);

    itemP = xmlrpc_array_get_item(&env, arrayP, 1);
    TEST_NO_FAULT(&env);
    {
        int i;
        xmlrpc_decompose_value(&env, itemP, "i", &i);
        TEST_NO_FAULT(&env);
        TEST(i == 10);
    }
    xmlrpc_decompose_value(&env, arrayP, "(nini)", &i1, &i2);
    TEST_NO_FAULT(&env);
    TEST(i1 == 10 && i2 == 20);

    /* Test bounds check on xmlrpc_array_get_item. */
    xmlrpc_array_read_item(&env, arrayP, 4, &itemP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_array_get_item(&env, arrayP, 4);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_DECREF(arrayP);

    xmlrpc_env_clean(&env);
}



static void
destroyMyCptr(void * const context,
              void * const objectP) {
/*----------------------------------------------------------------------------
   This is a xmlrpc_cptr_dtor_fn.
-----------------------------------------------------------------------------*/
    int * const destroyConfirmationP = context;
    int * const objectIntP = objectP;

    *destroyConfirmationP = *objectIntP;
}



static void
test_value_cptr(void) {

    int destroyConfirmation;

    xmlrpc_value * v;
    xmlrpc_env env;
    void * ptr;
    int myObject;

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_C_PTR), "C_PTR"));

    myObject = 7;

    v = xmlrpc_cptr_new(&env, &myObject);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_C_PTR);
    xmlrpc_DECREF(v);

    v = xmlrpc_cptr_new_dtor(&env, &myObject,
                             &destroyMyCptr, &destroyConfirmation);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_C_PTR);
    destroyConfirmation = 3;
    xmlrpc_DECREF(v);
    TEST(destroyConfirmation == 7);  // the destructor has set this
    
    v = xmlrpc_build_value(&env, "p", (void*) 0x00000017);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_C_PTR == xmlrpc_value_type(v));
    xmlrpc_decompose_value(&env, v, "p", &ptr);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(ptr == (void*) 0x00000017);

    xmlrpc_env_clean(&env);
}



static void
test_value_nil(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_NIL), "NIL"));

    v = xmlrpc_nil_new(&env);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_NIL);
    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "n");
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_NIL == xmlrpc_value_type(v));
    xmlrpc_decompose_value(&env, v, "n");
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);

    xmlrpc_env_clean(&env);
}



static void
test_value_i8(void) { 

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_int64 i;

    xmlrpc_env_init(&env);

    TEST(streq(xmlrpc_type_name(XMLRPC_TYPE_I8), "I8"));

    v = xmlrpc_i8_new(&env, (xmlrpc_int64) 25);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_I8);
    xmlrpc_read_i8(&env, v, &i);
    TEST_NO_FAULT(&env);
    TEST(i == 25);
    xmlrpc_DECREF(v);

    v = xmlrpc_i8_new(&env, (xmlrpc_int64) -25);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_I8);
    xmlrpc_read_i8(&env, v, &i);
    TEST_NO_FAULT(&env);
    TEST(i == -25);
    xmlrpc_DECREF(v);

    v = xmlrpc_i8_new(&env, (xmlrpc_int64)1 << 40);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_I8);
    xmlrpc_read_i8(&env, v, &i);
    TEST_NO_FAULT(&env);
    TEST(i == (xmlrpc_int64)1 << 40);
    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "I", (xmlrpc_int64) 10);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(xmlrpc_value_type(v) == XMLRPC_TYPE_I8);
    xmlrpc_decompose_value(&env, v, "I", &i);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(i == 10);

    xmlrpc_env_clean(&env);
}



static void
test_value_type_mismatch(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_env env2;
    char * str;

    /* Test for one, simple kind of type mismatch error. We assume that
    ** if one of these typechecks works, the rest work fine. */

    xmlrpc_env_init(&env);
    xmlrpc_env_init(&env2);

    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 5);
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env2, v, "s", &str);
    xmlrpc_DECREF(v);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);

    xmlrpc_env_clean(&env2);
    xmlrpc_env_clean(&env);
}



static void
test_value_invalid_type(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    /* Test invalid type specifier in format string */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "Q");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_env_clean(&env);
}



static void
test_value_missing_array_delim(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    /* Test missing close parenthesis on array */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "(");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    v = xmlrpc_build_value(&env, "(i");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_env_clean(&env);
}



static void
test_value_missing_struct_delim(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    
    /* Test missing closing brace on struct */

    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "{");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    v = xmlrpc_build_value(&env, "{s:i", "key1", 7);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    v = xmlrpc_build_value(&env, "{s:i,s:i", "key1", 9, "key2", -4);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_env_clean(&env);
}



static void
test_value_invalid_struct(void) {

    xmlrpc_value * v;
    xmlrpc_env env;

    /* Note that even though the format strings are invalid, we have
       to supply the variable arguments that xmlrpc_build_value() will
       be looking for as it tries to parse it.  Otherwise, we get wild
       memory references and consequent Valgrind flags.
    */
    
    xmlrpc_env_init(&env);

    v = xmlrpc_build_value(&env, "{s:ii", "key1", 9, 9);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    v = xmlrpc_build_value(&env, "{si:", "key1", 9);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    v = xmlrpc_build_value(&env, "{s", "key1");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_env_clean(&env);
}



static void
test_value_parse_value(void) {

    xmlrpc_env env;
    xmlrpc_value * valueP;
    const char * datestring = "19980717T14:08:55";

    xmlrpc_env_init(&env);

    valueP = xmlrpc_build_value(&env, "(idb8ss#6(i){s:i}np(i))",
                                7, 3.14, (xmlrpc_bool)1, datestring,
                                "hello world", "a\0b", (size_t)3,
                                "base64 data", strlen("base64 data"),
                                15, "member9", 9, &valueP, -5);
    
    TEST_NO_FAULT(&env);

    {
        xmlrpc_int32 i;
        xmlrpc_double d;
        xmlrpc_bool b;
        const char * dt_str;
        const char * s1;
        const char * s2;
        size_t s2_len;
        const unsigned char * b64;
        size_t b64_len;
        xmlrpc_value * arrayP;
        xmlrpc_value * structP;
        void * cptr;
        xmlrpc_value * subvalP;

        xmlrpc_parse_value(&env, valueP, "(idb8ss#6ASnpV)",
                           &i, &d, &b, &dt_str, &s1, &s2, &s2_len,
                           &b64, &b64_len,
                           &arrayP, &structP, &cptr, &subvalP);
        
        TEST_NO_FAULT(&env);

        TEST(i == 7);
        TEST(d == 3.14);
        TEST(b == (xmlrpc_bool)1);
        TEST(streq(dt_str, datestring));
        TEST(streq(s1, "hello world"));
        TEST(s2_len == 3);
        TEST(memeq(s2, "a\0b", 3));
        TEST(b64_len == strlen("base64 data"));
        TEST(memeq(b64, "base64 data", b64_len));
        TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(arrayP));
        TEST(XMLRPC_TYPE_STRUCT == xmlrpc_value_type(structP));
        TEST(cptr == &valueP);
        TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(subvalP));

        xmlrpc_DECREF(valueP);
    }
    xmlrpc_env_clean(&env);
}



static void
test_struct_get_element(xmlrpc_value * const structP,
                        xmlrpc_value * const fooValueP,
                        xmlrpc_value * const weirdValueP,
                        const char *   const weirdKey,
                        unsigned int   const weirdKeyLen) {

    xmlrpc_env env;
    xmlrpc_value * valueP;
    xmlrpc_value * fooStringP;
    xmlrpc_value * bogusKeyStringP;

    xmlrpc_env_init(&env);

    /* build test tools */

    fooStringP = xmlrpc_build_value(&env, "s", "foo");
    TEST_NO_FAULT(&env);

    bogusKeyStringP = xmlrpc_build_value(&env, "s", "doesn't_exist");
    TEST_NO_FAULT(&env);

    /* "find" interface */

    xmlrpc_struct_find_value(&env, structP, "foo", &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == fooValueP);
    xmlrpc_DECREF(valueP);
            
    xmlrpc_struct_find_value(&env, structP, "doesn't_exist", &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == NULL);
            
    xmlrpc_struct_find_value_v(&env, structP, fooStringP, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == fooValueP);
    xmlrpc_DECREF(valueP);
            
    xmlrpc_struct_find_value_v(&env, structP, bogusKeyStringP, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == NULL);

    xmlrpc_struct_find_value(&env, fooValueP, "foo", &valueP);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
            
    /* "read" interface */
            
    xmlrpc_struct_read_value(&env, structP, "foo", &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == fooValueP);
    xmlrpc_DECREF(valueP);
            
    xmlrpc_struct_read_value(&env, structP, "doesn't_exist", &valueP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
            
    xmlrpc_struct_read_value_v(&env, structP, fooStringP, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == fooValueP);
    xmlrpc_DECREF(valueP);
            
    xmlrpc_struct_read_value_v(&env, structP, bogusKeyStringP, &valueP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    xmlrpc_struct_read_value(&env, fooValueP, "foo", &valueP);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
            
    /* obsolete "get" interface.  Note that it does not update the
       reference count of the xmlrpc_value it returns.
    */
            
    valueP = xmlrpc_struct_get_value(&env, structP, "foo");
    TEST_NO_FAULT(&env);
    TEST(valueP == fooValueP);

    valueP = xmlrpc_struct_get_value(&env, structP, "doesn't_exist");
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    valueP = xmlrpc_struct_get_value(&env, fooValueP, "foo");
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    valueP = xmlrpc_struct_get_value_n(&env, structP, weirdKey, weirdKeyLen);
    TEST_NO_FAULT(&env);
    TEST(valueP == weirdValueP);

    /* Clean up */

    xmlrpc_DECREF(fooStringP);
    xmlrpc_DECREF(bogusKeyStringP);

    xmlrpc_env_clean(&env);
}



static void
testStructReadout(xmlrpc_value * const structP,
                  unsigned int   const expectedSize) {

    xmlrpc_env env;
    xmlrpc_value * keyP;
    xmlrpc_value * valueP;

    unsigned int index;

    xmlrpc_env_init(&env);

    for (index = 0; index < expectedSize; ++index) {
        xmlrpc_struct_read_member(&env, structP, index, &keyP, &valueP);
        TEST_NO_FAULT(&env);
        xmlrpc_DECREF(keyP);
        xmlrpc_DECREF(valueP);
    }

    xmlrpc_struct_read_member(&env, structP, expectedSize, &keyP, &valueP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    for (index = 0; index < expectedSize; ++index) {
        xmlrpc_struct_get_key_and_value(&env, structP, index, &keyP, &valueP);
        TEST_NO_FAULT(&env);
    }
    xmlrpc_env_clean(&env);
}



static void
test_struct_decompose_invalid_format_string(
    xmlrpc_value * const testStructP) {

    xmlrpc_env env;
    xmlrpc_int32 ival;
    xmlrpc_bool bval;
    char * sval;

    xmlrpc_env_init(&env);

    /* Premature end of format string: */

    xmlrpc_decompose_value(&env, testStructP, "{");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    
    xmlrpc_decompose_value(&env, testStructP, "{s", "baz");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_decompose_value(&env, testStructP, "{s:", "baz");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    
    xmlrpc_decompose_value(&env, testStructP, "{s:", "baz");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    
    xmlrpc_decompose_value(&env, testStructP, "{s:b", "baz", &bval);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    
    xmlrpc_decompose_value(&env, testStructP, "{s:b,", "baz", &bval);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    
    xmlrpc_decompose_value(&env, testStructP, "{s:b,*", "baz", &bval);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    /* Key not 's' */
    xmlrpc_decompose_value(&env, testStructP, "{i:s,s:i,*}",
                           "baz", &sval,
                           "foo", &ival);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    /* Missing colon */
    xmlrpc_decompose_value(&env, testStructP, "{is,s:i,*}",
                           "baz", &sval,
                           "foo", &ival);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    /* Missing comma */
    xmlrpc_decompose_value(&env, testStructP, "{i:ss:i,*}",
                           "baz", &sval,
                           "foo", &ival);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    /* Undefined format specifier */
    xmlrpc_decompose_value(&env, testStructP, "{s:Q,*}",
                           "baz", &sval);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    /* No * at end */
    xmlrpc_decompose_value(&env, testStructP, "{s:b,s:s,s:i}",
                           "baz", &bval,
                           "foo", &sval,
                           "bar", &ival);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);

    xmlrpc_env_clean(&env);
}



static void
test_struct_decompose(xmlrpc_value * const testStructP) {

    xmlrpc_env env;

    xmlrpc_int32 ival;
    xmlrpc_bool bval;
    char * sval;
    xmlrpc_value * topStructP;
    xmlrpc_value * value1P;

    xmlrpc_env_init(&env);

    /* Make a test struct */
    topStructP = xmlrpc_build_value(&env, "{s:S,s:s}",
                                    "key1", testStructP,
                                    "key2", "my_string_value");
    TEST_NO_FAULT(&env);

    test_struct_decompose_invalid_format_string(testStructP);

    /* Decompose a struct */
    xmlrpc_decompose_value(&env, testStructP, "{s:b,s:s,s:i,*}",
                           "baz", &bval,
                           "foo", &sval,
                           "bar", &ival);
    TEST_NO_FAULT(&env);
    TEST(ival == 1);
    TEST(!bval);
    TEST(streq(sval, "Hello!"));
    free(sval);

    /* Decompose a deep struct */

    xmlrpc_decompose_value(&env, topStructP, "{s:S,*}", "key1", &value1P);
    TEST_NO_FAULT(&env);
    TEST(xmlrpc_value_type(value1P) == XMLRPC_TYPE_STRUCT);
    xmlrpc_DECREF(value1P);

    xmlrpc_decompose_value(&env, topStructP, "{s:{s:b,s:s,s:i,*},*}",
                           "key1",
                           "baz", &bval,
                           "foo", &sval,
                           "bar", &ival);
    TEST_NO_FAULT(&env);
    TEST(ival == 1);
    TEST(!bval);
    TEST(streq(sval, "Hello!"));
    free(sval);

    /* First value of wrong type */
    xmlrpc_decompose_value(&env, testStructP, "{s:b,s:i,*}",
                           "baz", &sval,
                           "foo", &ival);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    /* Subsequent value of wrong type */
    xmlrpc_decompose_value(&env, testStructP, "{s:s,s:i,*}",
                           "foo", &sval,
                           "baz", &bval);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    /* Nonexistent key */
    xmlrpc_decompose_value(&env, testStructP, "{s:b,s:i,*}",
                           "baz", &bval,
                           "nosuch", &sval);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);

    /* Error subsequent to nested value */
    xmlrpc_decompose_value(&env, topStructP, "{s:{s:s,*},s:i,*}",
                           "key1",
                           "foo", &sval,
                           "key2", &ival);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    xmlrpc_DECREF(topStructP);

    xmlrpc_env_clean(&env);
}



static void
test_struct (void) {

    xmlrpc_env env;
    xmlrpc_value * value1P;
    xmlrpc_value *s, *i, *i1, *i2, *i3, *key, *value;
    size_t size;
    int present;
    xmlrpc_bool bval;
    char const weirdKey[] = {'f', 'o', 'o', '\0', 'b', 'a', 'r'};

    xmlrpc_env_init(&env);

    /* Create a struct. */
    s = xmlrpc_struct_new(&env);
    TEST_NO_FAULT(&env);
    TEST(s != NULL);
    TEST(XMLRPC_TYPE_STRUCT == xmlrpc_value_type(s));
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 0);

    /* Create some elements to insert into our struct. */
    i1 = xmlrpc_build_value(&env, "s", "Item #1");
    TEST_NO_FAULT(&env);
    i2 = xmlrpc_build_value(&env, "s", "Item #2");
    TEST_NO_FAULT(&env);
    i3 = xmlrpc_build_value(&env, "s", "Item #3");
    TEST_NO_FAULT(&env);

    /* Insert a single item. */
    xmlrpc_struct_set_value(&env, s, "foo", i1);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 1);

    /* Insert an item whose key has the same hash value as "foo". */
    xmlrpc_struct_set_value(&env, s, "qmdebdw", i2);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 2);
    i = xmlrpc_struct_get_value(&env, s, "foo");
    TEST_NO_FAULT(&env);
    TEST(i == i1);
    i = xmlrpc_struct_get_value(&env, s, "qmdebdw");
    TEST_NO_FAULT(&env);
    TEST(i == i2);

    /* Replace an existing element with a different element. */
    xmlrpc_struct_set_value(&env, s, "foo", i3);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 2);
    i = xmlrpc_struct_get_value(&env, s, "foo");
    TEST_NO_FAULT(&env);
    TEST(i == i3);

    /* Insert an item with a NUL in the key */
    xmlrpc_struct_set_value_n(&env, s, weirdKey, sizeof(weirdKey), i2);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);

    test_struct_get_element(s, i3, i2, weirdKey, sizeof(weirdKey));

    /* Replace an existing element with the same element (tricky). */
    xmlrpc_struct_set_value(&env, s, "foo", i3);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);
    i = xmlrpc_struct_get_value(&env, s, "foo");
    TEST_NO_FAULT(&env);
    TEST(i == i3);

    /* Test for the presence and absence of elements. */
    present = xmlrpc_struct_has_key(&env, s, "foo");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "qmdebdw");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "bogus");
    TEST_NO_FAULT(&env);
    TEST(!present);

    /* Make sure our typechecks work correctly. */
    xmlrpc_struct_size(&env, i1);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    xmlrpc_struct_has_key(&env, i1, "foo");
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    xmlrpc_struct_set_value(&env, i1, "foo", i2);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    xmlrpc_struct_set_value_v(&env, s, s, i2);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);

    /* Test cleanup code (w/memprof). */
    xmlrpc_DECREF(s);

    s = xmlrpc_build_value(&env, "{s:s,s:i,s:b}",
                           "foo", "Hello!",
                           "bar", (xmlrpc_int32) 1,
                           "baz", (xmlrpc_bool) 0);
    TEST_NO_FAULT(&env);
    TEST(s != NULL);
    TEST(xmlrpc_value_type(s) == XMLRPC_TYPE_STRUCT);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);
    present = xmlrpc_struct_has_key(&env, s, "foo");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "bar");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "baz");
    TEST_NO_FAULT(&env);
    TEST(present);
    xmlrpc_struct_read_value(&env, s, "baz", &value1P);
    TEST_NO_FAULT(&env);
    xmlrpc_read_bool(&env, value1P, &bval);
    TEST_NO_FAULT(&env);
    TEST(!bval);
    xmlrpc_DECREF(value1P);

    testStructReadout(s, 3);

    test_struct_decompose(s);

    /* Test type check. */
    xmlrpc_struct_get_key_and_value(&env, i1, 0, &key, &value);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
    TEST(key == NULL && value == NULL);
    
    /* Test bounds checks. */
    xmlrpc_struct_get_key_and_value(&env, s, -1, &key, &value);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    TEST(key == NULL && value == NULL);

    xmlrpc_struct_get_key_and_value(&env, s, 3, &key, &value);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    TEST(key == NULL && value == NULL);
    
    /* Test cleanup code (w/memprof). */
    xmlrpc_DECREF(s);

    xmlrpc_DECREF(i1);
    xmlrpc_DECREF(i2);
    xmlrpc_DECREF(i3);
    xmlrpc_env_clean(&env);
}



void 
test_value(void) {

    printf("Running value tests.");

    test_value_alloc_dealloc();
    test_value_int();
    test_value_bool();
    test_value_double();
    test_value_datetime();
    test_value_string_no_null();
    test_value_string_null();
    test_value_string_multiline();
    test_value_string_cr();
    test_value_string_wide();
    test_value_base64();
    test_value_array();
    test_value_array2();
    test_value_array_nil();
    test_value_value();
    test_value_AS();
    test_value_AS_typecheck();
    test_value_cptr();
    test_value_nil();
    test_value_i8();
    test_value_type_mismatch();
    test_value_invalid_type();
    test_value_missing_array_delim();
    test_value_missing_struct_delim();
    test_value_invalid_struct();
    test_value_parse_value();
    test_struct();

    printf("\n");
    printf("Value tests done.\n");
}
