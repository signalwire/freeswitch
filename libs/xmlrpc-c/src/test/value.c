/* Copyright information is at the end of the file. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"

#include "test.h"
#include "value.h"



/*=========================================================================
**  Test Data
**=========================================================================
**  Some common test data which need to be allocated at a fixed address,
**  or which are inconvenient to allocate inline.
*/
static char* test_string_1 = "foo";



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
test_value_integer(void) { 

    xmlrpc_value * v;
    xmlrpc_env env;
    xmlrpc_int32 i;

    xmlrpc_env_init(&env);

    v = xmlrpc_int_new(&env, (xmlrpc_int32) 25);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_INT == xmlrpc_value_type(v));
    xmlrpc_read_int(&env, v, &i);
    TEST_NO_FAULT(&env);
    TEST(i == 25);
    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 10);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_INT == xmlrpc_value_type(v));
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



static void
test_value_datetime(void) {

    const char * datestring = "19980717T14:08:55";
    time_t const datetime = 900684535;

    xmlrpc_value * v;
    xmlrpc_env env;
    const char * ds;
    time_t dt;

    xmlrpc_env_init(&env);

    v = xmlrpc_datetime_new_str(&env, datestring);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));

    xmlrpc_read_datetime_str(&env, v, &ds);
    TEST_NO_FAULT(&env);
    TEST(strcmp(ds, datestring) == 0);
    strfree(ds);

    xmlrpc_read_datetime_sec(&env, v, &dt);
    TEST_NO_FAULT(&env);
    TEST(dt == datetime);

    xmlrpc_DECREF(v);

    v = xmlrpc_datetime_new_sec(&env, datetime);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));

    xmlrpc_read_datetime_str(&env, v, &ds);
    TEST_NO_FAULT(&env);
    TEST(strcmp(ds, datestring) == 0);
    strfree(ds);

    xmlrpc_read_datetime_sec(&env, v, &dt);
    TEST_NO_FAULT(&env);
    TEST(dt == datetime);

    xmlrpc_DECREF(v);

    v = xmlrpc_build_value(&env, "8", datestring);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_DATETIME == xmlrpc_value_type(v));
    xmlrpc_decompose_value(&env, v, "8", &ds);
    xmlrpc_DECREF(v);
    TEST_NO_FAULT(&env);
    TEST(strcmp(ds, datestring) == 0);
    strfree(ds);

    xmlrpc_env_clean(&env);
}



static void
test_value_string_no_null(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    const char * str;
    size_t len;

    /* Test strings (without '\0' bytes). */
    xmlrpc_env_init(&env);

    v = xmlrpc_string_new(&env, test_string_1);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));
    xmlrpc_read_string(&env, v, &str);
    TEST_NO_FAULT(&env);
    TEST(strcmp(str, test_string_1) == 0);
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_build_value(&env, "s", test_string_1);

    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));
    xmlrpc_decompose_value(&env, v, "s", &str);
    TEST_NO_FAULT(&env);
    TEST(strcmp(str, test_string_1) == 0);
    xmlrpc_decompose_value(&env, v, "s#", &str, &len);
    TEST_NO_FAULT(&env);
    TEST(memcmp(str, test_string_1, strlen(test_string_1)) == 0);
    TEST(strlen(str) == strlen(test_string_1));
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
    TEST(memcmp(str, "foo\0bar", 7) == 0);
    xmlrpc_DECREF(v);
    strfree(str);

    v = xmlrpc_build_value(&env, "s#", "foo\0bar", (size_t) 7);
    TEST_NO_FAULT(&env);
    TEST(v != NULL);
    TEST(XMLRPC_TYPE_STRING == xmlrpc_value_type(v));

    xmlrpc_decompose_value(&env, v, "s#", &str, &len);
    TEST_NO_FAULT(&env);
    TEST(memcmp(str, "foo\0bar", 7) == 0);
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



#ifdef HAVE_UNICODE_WCHAR

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
    valueP = xmlrpc_build_value(&env, "w#", wcs_data, 3);
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    /* Verify it */
    xmlrpc_read_string_w_lp(&env, valueP, &len, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(0 == wcsncmp(wcs, wcs_data, len));
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
    TEST(0 == wcsncmp(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_DECREF(valueP);
}



static void 
test_value_string_wide(void) {
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
    TEST(0 == wcsncmp(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_read_string_w(&env, valueP, &wcs);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(wcs[3] == '\0');
    TEST(0 == wcsncmp(wcs, wcs_data, 3));
    free((void*)wcs);

    xmlrpc_decompose_value(&env, valueP, "w#", &wcs, &len);
    TEST_NO_FAULT(&env);
    TEST(wcs != NULL);
    TEST(len == 3);
    TEST(wcs[len] == '\0');
    TEST(0 == wcsncmp(wcs, wcs_data, len));
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
        TEST(0 == strncmp(str, utf8_data, len));
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
    TEST(0 == wcsncmp(wcs, wcs_data, len));
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
    TEST(0 == wcsncmp(wcs, wcs_data, len));
    free((void*)wcs);

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
    TEST(0 == wcsncmp(wcs, wcs_data, len));
    free((void*)wcs);

    xmlrpc_read_string_w(&env, valueP, &wcs);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
}
#else
static void 
test_value_string_wide(void) {}
#endif



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

    v = xmlrpc_base64_new(&env, sizeof(data1), data1);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_BASE64 == xmlrpc_value_type(v));
    xmlrpc_read_base64(&env, v, &len, &data);
    TEST_NO_FAULT(&env);
    TEST(memcmp(data, data1, sizeof(data1)) == 0);
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
    TEST(memcmp(data, data1, sizeof(data2)) == 0);
    strfree(data);

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

    /* Basic array-building test. */

    xmlrpc_env_init(&env);

    v = xmlrpc_array_new(&env);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_ARRAY == xmlrpc_value_type(v));
    len = xmlrpc_array_size(&env, v);
    TEST_NO_FAULT(&env);
    TEST(len == 0);
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

    v = xmlrpc_build_value(&env, "s", "foo");
    TEST_NO_FAULT(&env);
    xmlrpc_env_init(&env2);
    xmlrpc_decompose_value(&env2, v, "A", &v2);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_decompose_value(&env2, v, "S", &v2);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);
    xmlrpc_DECREF(v);
    xmlrpc_env_clean(&env);
}



static void
test_value_array2(void) {

    xmlrpc_value * arrayP;
    xmlrpc_env env;
    xmlrpc_int32 i, i1, i2, i3, i4;
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


    /* Test bounds check on xmlrpc_array_get_item. */
    xmlrpc_array_read_item(&env, arrayP, 3, &itemP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    xmlrpc_array_get_item(&env, arrayP, 3);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    xmlrpc_array_get_item(&env, arrayP, -1);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

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
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    xmlrpc_array_get_item(&env, arrayP, 4);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    xmlrpc_DECREF(arrayP);

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

    v = xmlrpc_build_value(&env, "i", (xmlrpc_int32) 5);
    TEST_NO_FAULT(&env);
    xmlrpc_env_init(&env2);
    xmlrpc_decompose_value(&env2, v, "s", &str);
    xmlrpc_DECREF(v);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_clean(&env);
}



static void
test_value_cptr(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    void * ptr;

    /* Test C pointer storage using 'p'.
       We don't have cleanup functions (yet). 
    */

    xmlrpc_env_init(&env);

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

    v = xmlrpc_nil_new(&env);
    TEST_NO_FAULT(&env);
    TEST(XMLRPC_TYPE_NIL == xmlrpc_value_type(v));
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
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
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
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s:i", "key1", 7);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s:i,s:i", "key1", 9, "key2", -4);
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);
}



static void
test_value_invalid_struct(void) {

    xmlrpc_value * v;
    xmlrpc_env env;
    
    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s:ii");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{si:");
    TEST_FAULT(&env, XMLRPC_INTERNAL_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);
    v = xmlrpc_build_value(&env, "{s");
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
                                "hello world", "a\0b", 3, 
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
        TEST(strcmp(dt_str, datestring) == 0);
        TEST(strcmp(s1, "hello world") == 0);
        TEST(s2_len == 3);
        TEST(memcmp(s2, "a\0b", 3) == 0);
        TEST(b64_len == strlen("base64 data"));
        TEST(memcmp(b64, "base64 data", b64_len) == 0);
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
                        xmlrpc_value * const i1,
                        xmlrpc_value * const i2,
                        const char *   const weirdKey,
                        unsigned int   const weirdKeyLen) {

    xmlrpc_env env;
    xmlrpc_value * valueP;
    xmlrpc_value * aasStringP;
    xmlrpc_value * bogusKeyStringP;

    xmlrpc_env_init(&env);

    /* build test tools */

    aasStringP = xmlrpc_build_value(&env, "s", "aas");
    TEST_NO_FAULT(&env);

    bogusKeyStringP = xmlrpc_build_value(&env, "s", "doesn't_exist");
    TEST_NO_FAULT(&env);

    /* "find" interface */

    xmlrpc_struct_find_value(&env, structP, "aas", &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == i1);
            
    xmlrpc_struct_find_value(&env, structP, "doesn't_exist", &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == NULL);
            
    xmlrpc_struct_find_value_v(&env, structP, aasStringP, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == i1);
    xmlrpc_DECREF(valueP);
            
    xmlrpc_struct_find_value_v(&env, structP, bogusKeyStringP, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == NULL);

    xmlrpc_struct_find_value(&env, i1, "aas", &valueP);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);
            
    /* "read" interface */
            
    xmlrpc_struct_read_value(&env, structP, "aas", &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == i1);
    xmlrpc_DECREF(valueP);
            
    xmlrpc_struct_read_value(&env, structP, "doesn't_exist", &valueP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);
            
    xmlrpc_struct_read_value_v(&env, structP, aasStringP, &valueP);
    TEST_NO_FAULT(&env);
    TEST(valueP == i1);
    xmlrpc_DECREF(valueP);
            
    xmlrpc_struct_read_value_v(&env, structP, bogusKeyStringP, &valueP);
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    xmlrpc_struct_read_value(&env, i1, "aas", &valueP);
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);
            
    /* obsolete "get" interface */
            
    valueP = xmlrpc_struct_get_value(&env, structP, "aas");
    TEST_NO_FAULT(&env);
    TEST(valueP == i1);

    valueP = xmlrpc_struct_get_value(&env, structP, "doesn't_exist");
    TEST_FAULT(&env, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    valueP = xmlrpc_struct_get_value(&env, i1, "foo");
    TEST_FAULT(&env, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    valueP = xmlrpc_struct_get_value_n(&env, structP, weirdKey, weirdKeyLen);
    TEST_NO_FAULT(&env);
    TEST(valueP == i2);

    /* Clean up */

    xmlrpc_DECREF(aasStringP);
    xmlrpc_DECREF(bogusKeyStringP);

    xmlrpc_env_clean(&env);
}



static void
testStructReadout(xmlrpc_value * const structP,
                  size_t         const expectedSize) {

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
    xmlrpc_env_clean(&env);
    xmlrpc_env_init(&env);

    for (index = 0; index < expectedSize; ++index) {
        xmlrpc_struct_get_key_and_value(&env, structP, index, &keyP, &valueP);
        TEST_NO_FAULT(&env);
    }
    xmlrpc_env_clean(&env);
}



static void
test_struct (void) {

    xmlrpc_env env, env2;
    xmlrpc_value *s, *i, *i1, *i2, *i3, *key, *value;
    size_t size;
    int present;
    xmlrpc_int32 ival;
    xmlrpc_bool bval;
    char *sval;
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

    /* Insert two more items with conflicting hash codes. (We assume that
    ** nobody has changed the hash function.) */
    xmlrpc_struct_set_value(&env, s, "bar", i2);
    TEST_NO_FAULT(&env);
    xmlrpc_struct_set_value(&env, s, "aas", i3);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);

    /* Replace an existing element with a different element. */
    xmlrpc_struct_set_value(&env, s, "aas", i1);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 3);

    /* Insert an item with a NUL in the key */
    xmlrpc_struct_set_value_n(&env, s, weirdKey, sizeof(weirdKey), i2);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 4);

    test_struct_get_element(s, i1, i2, weirdKey, sizeof(weirdKey));

    /* Replace an existing element with the same element (tricky). */
    xmlrpc_struct_set_value(&env, s, "aas", i1);
    TEST_NO_FAULT(&env);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 4);
    i = xmlrpc_struct_get_value(&env, s, "aas");
    TEST_NO_FAULT(&env);
    TEST(i == i1);

    /* Test for the presence and absence of elements. */
    present = xmlrpc_struct_has_key(&env, s, "aas");
    TEST_NO_FAULT(&env);
    TEST(present);
    present = xmlrpc_struct_has_key(&env, s, "bogus");
    TEST_NO_FAULT(&env);
    TEST(!present);

    /* Make sure our typechecks work correctly. */
    xmlrpc_env_init(&env2);
    xmlrpc_struct_size(&env2, i1);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_has_key(&env2, i1, "foo");
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_set_value(&env2, i1, "foo", i2);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_set_value_v(&env2, s, s, i2);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    /* Test cleanup code (w/memprof). */
    xmlrpc_DECREF(s);

    /* Build a struct using our automagic struct builder. */
    s = xmlrpc_build_value(&env, "{s:s,s:i,s:b}",
                           "foo", "Hello!",
                           "bar", (xmlrpc_int32) 1,
                           "baz", (xmlrpc_bool) 0);
    TEST_NO_FAULT(&env);
    TEST(s != NULL);
    TEST(XMLRPC_TYPE_STRUCT == xmlrpc_value_type(s));
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
    i = xmlrpc_struct_get_value(&env, s, "baz");
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, i, "b", &bval);
    TEST_NO_FAULT(&env);
    TEST(!bval);

    testStructReadout(s, 3);

    /* Test our automagic struct parser. */
    xmlrpc_decompose_value(&env, s, "{s:b,s:s,s:i,*}",
                           "baz", &bval,
                           "foo", &sval,
                           "bar", &ival);
    TEST_NO_FAULT(&env);
    TEST(ival == 1);
    TEST(!bval);
    TEST(strcmp(sval, "Hello!") == 0);
    free(sval);

    /* Test automagic struct parser with value of wrong type. */
    xmlrpc_env_init(&env2);
    xmlrpc_decompose_value(&env2, s, "{s:b,s:i,*}",
                           "baz", &bval,
                           "foo", &sval);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    xmlrpc_env_clean(&env2);

    /* Test automagic struct parser with bad key. */
    xmlrpc_env_init(&env2);
    xmlrpc_decompose_value(&env2, s, "{s:b,s:i,*}",
                           "baz", &bval,
                           "nosuch", &sval);
    TEST_FAULT(&env2, XMLRPC_INDEX_ERROR);
    xmlrpc_env_clean(&env2);

    /* Test type check. */
    xmlrpc_env_init(&env2);
    xmlrpc_struct_get_key_and_value(&env2, i1, 0, &key, &value);
    TEST_FAULT(&env2, XMLRPC_TYPE_ERROR);
    TEST(key == NULL && value == NULL);
    xmlrpc_env_clean(&env2);
    
    /* Test bounds checks. */
    xmlrpc_env_init(&env2);
    xmlrpc_struct_get_key_and_value(&env2, s, -1, &key, &value);
    TEST_FAULT(&env2, XMLRPC_INDEX_ERROR);
    TEST(key == NULL && value == NULL);
    xmlrpc_env_clean(&env2);

    xmlrpc_env_init(&env2);
    xmlrpc_struct_get_key_and_value(&env2, s, 3, &key, &value);
    TEST_FAULT(&env2, XMLRPC_INDEX_ERROR);
    TEST(key == NULL && value == NULL);
    xmlrpc_env_clean(&env2);
    
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
    test_value_integer();
    test_value_bool();
    test_value_double();
    test_value_datetime();
    test_value_string_no_null();
    test_value_string_null();
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
