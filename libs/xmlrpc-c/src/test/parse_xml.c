#include <limits.h>
#include <stdlib.h>

#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/xmlparser.h"

#include "test.h"
#include "xml_data.h"
#include "parse_xml.h"



static void test_expat (void)
{
    xmlrpc_env env;
    xml_element *elem, *array, *data, *value1, *i4;
    char *cdata;
    size_t size;

    xmlrpc_env_init(&env);

    /* Parse a moderately complex XML document. */
    elem = xml_parse(&env, expat_data, strlen(expat_data));
    TEST_NO_FAULT(&env);
    TEST(elem != NULL);

    /* Verify our results. */
    TEST(strcmp(xml_element_name(elem), "value") == 0);
    TEST(xml_element_children_size(elem) == 1);
    array = xml_element_children(elem)[0];
    TEST(strcmp(xml_element_name(array), "array") == 0);
    TEST(xml_element_children_size(array) == 1);
    data = xml_element_children(array)[0];
    TEST(strcmp(xml_element_name(data), "data") == 0);
    TEST(xml_element_children_size(data) > 1);
    value1 = xml_element_children(data)[0];
    TEST(strcmp(xml_element_name(value1), "value") == 0);
    TEST(xml_element_children_size(value1) == 1);
    i4 = xml_element_children(value1)[0];
    TEST(strcmp(xml_element_name(i4), "i4") == 0);
    TEST(xml_element_children_size(i4) == 0);
    cdata = xml_element_cdata(i4);
    size = xml_element_cdata_size(i4);
    TEST(size == strlen("2147483647"));
    TEST(memcmp(cdata, "2147483647", strlen("2147483647")) == 0);

    /* Test cleanup code (w/memprof). */
    xml_element_free(elem);

    /* Try to parse broken XML. We want to know that a proper error occurs,
    ** AND that we don't leak any memory (w/memprof). */
    elem = xml_parse(&env, expat_error_data, strlen(expat_error_data));
    TEST(env.fault_occurred);
    TEST(elem == NULL);

    xmlrpc_env_clean(&env);
}



static void
test_parse_xml_value(void) {

    xmlrpc_env env, env2;
    xmlrpc_value *s, *sval;
    xmlrpc_int32 int_max, int_min, int_one;
    xmlrpc_bool bool_false, bool_true;
    char *str_hello, *str_untagged, *datetime;
    unsigned char *b64_data;
    size_t b64_len;
    double negone, zero, one;
    int size, sval_int;
    const char **bad_value;
    
    xmlrpc_env_init(&env);
    
    {
        xmlrpc_value * valueP;

        /* Parse a correctly-formed response. */
        valueP = xmlrpc_parse_response(&env, correct_value,
                                       strlen(correct_value));
        TEST_NO_FAULT(&env);
        TEST(valueP != NULL);
        
        /* Analyze it and make sure it contains the correct values. */
        xmlrpc_decompose_value(
            &env, valueP, "((iibbs68())idddSs)", 
            &int_max, &int_min,
            &bool_false, &bool_true, &str_hello,
            &b64_data, &b64_len, &datetime,
            &int_one, &negone, &zero, &one, &s, &str_untagged);

        xmlrpc_DECREF(valueP);
    }    
    TEST_NO_FAULT(&env);
    TEST(int_max == INT_MAX);
    TEST(int_min == INT_MIN);
    TEST(!bool_false);
    TEST(bool_true);
    TEST(strlen(str_hello) == strlen("Hello, world! <&>"));
    TEST(strcmp(str_hello, "Hello, world! <&>") == 0);
    TEST(b64_len == 11);
    TEST(memcmp(b64_data, "base64 data", b64_len) == 0);
    TEST(strcmp(datetime, "19980717T14:08:55") == 0);
    TEST(int_one == 1);
    TEST(negone == -1.0);
    TEST(zero == 0.0);
    TEST(one == 1.0);
    TEST(strcmp(str_untagged, "Untagged string") == 0);
    free(str_hello);
    free(b64_data);
    free(datetime);
    free(str_untagged);
    
    /* Analyze the contents of our struct. */
    TEST(s != NULL);
    size = xmlrpc_struct_size(&env, s);
    TEST_NO_FAULT(&env);
    TEST(size == 2);
    sval = xmlrpc_struct_get_value(&env, s, "ten <&>");
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, sval, "i", &sval_int);
    TEST_NO_FAULT(&env);
    TEST(sval_int == 10);
    sval = xmlrpc_struct_get_value(&env, s, "twenty");
    TEST_NO_FAULT(&env);
    xmlrpc_decompose_value(&env, sval, "i", &sval_int);
    TEST_NO_FAULT(&env);
    TEST(sval_int == 20);
    xmlrpc_DECREF(s);
    
    /* Test our error-checking code. This is exposed to potentially-malicious
    ** network data, so we need to handle evil data gracefully, without
    ** barfing or leaking memory. (w/memprof) */
    
    {
        xmlrpc_value * valueP;

        /* First, test some poorly-formed XML data. */
        xmlrpc_env_init(&env2);
        valueP = xmlrpc_parse_response(&env2, unparseable_value,
                                       strlen(unparseable_value));
        TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
        TEST(valueP == NULL);
        xmlrpc_env_clean(&env2);
    }
    /* Next, check for bogus values. These are all well-formed XML, but
    ** they aren't legal XML-RPC. */
    for (bad_value = bad_values; *bad_value != NULL; bad_value++) {
        xmlrpc_value * valueP;
        xml_element *elem;
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        elem = xml_parse(&env, *bad_value, strlen(*bad_value));
        TEST_NO_FAULT(&env);
        xml_element_free(elem);
    
        /* Now, make sure the higher-level routine barfs appropriately. */
        xmlrpc_env_init(&env2);
        valueP = xmlrpc_parse_response(&env2, *bad_value, strlen(*bad_value));
        TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
        TEST(valueP == NULL);
        xmlrpc_env_clean(&env2);
    }
    
    xmlrpc_env_clean(&env);
}

static void test_parse_xml_response (void)
{
    xmlrpc_env env, env2;
    int i1;
    const char **bad_resp;

    xmlrpc_env_init(&env);

    {
        xmlrpc_value * v;
        /* Parse a valid response. */
        v = xmlrpc_parse_response(&env, serialized_response,
                                  strlen(serialized_response));
        TEST_NO_FAULT(&env);
        TEST(v != NULL);
        xmlrpc_decompose_value(&env, v, "i", &i1);
        xmlrpc_DECREF(v);
    }
    TEST_NO_FAULT(&env);
    TEST(i1 == 30);

    {
        xmlrpc_value * v;
        xmlrpc_env fault;

        /* Parse a valid fault. */
        xmlrpc_env_init(&fault);
        v = xmlrpc_parse_response(&fault, serialized_fault,
                                  strlen(serialized_fault));

        TEST(fault.fault_occurred);
        TEST(fault.fault_code == 6);
        TEST(strcmp(fault.fault_string, "A fault occurred") == 0);
        xmlrpc_env_clean(&fault);
    }
    /* We don't need to test our handling of poorly formatted XML here,
    ** because we already did that in test_parse_xml_value. */

    /* Next, check for bogus responses. These are all well-formed XML, but
    ** they aren't legal XML-RPC. */
    for (bad_resp = bad_responses; *bad_resp != NULL; bad_resp++) {
        xmlrpc_value * v;
        xml_element *elem;
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        elem = xml_parse(&env, *bad_resp, strlen(*bad_resp));
        TEST_NO_FAULT(&env);
        xml_element_free(elem);
    
        /* Now, make sure the higher-level routine barfs appropriately. */
        xmlrpc_env_init(&env2);
        v = xmlrpc_parse_response(&env2, *bad_resp, strlen(*bad_resp));
        TEST(env2.fault_occurred);
        TEST(env2.fault_code != 0); /* We use 0 as a code in our bad faults. */
        TEST(v == NULL);
        xmlrpc_env_clean(&env2);
    }
    
    xmlrpc_env_clean(&env);
}

static void test_parse_xml_call (void)
{
    xmlrpc_env env, env2;
    const char *method_name;
    xmlrpc_value *params;
    int i1, i2;
    const char **bad_call;
    xml_element *elem;

    xmlrpc_env_init(&env);

    /* Parse a valid call. */
    xmlrpc_parse_call(&env, serialized_call, strlen(serialized_call),
                      &method_name, &params);
    TEST_NO_FAULT(&env);
    TEST(params != NULL);
    xmlrpc_decompose_value(&env, params, "(ii)", &i1, &i2);
    xmlrpc_DECREF(params);    
    TEST_NO_FAULT(&env);
    TEST(strcmp(method_name, "gloom&doom") == 0);
    TEST(i1 == 10 && i2 == 20);
    strfree(method_name);

    /* Test some poorly-formed XML data. */
    xmlrpc_env_init(&env2);
    xmlrpc_parse_call(&env2, unparseable_value, strlen(unparseable_value),
                      &method_name, &params);
    TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
    TEST(method_name == NULL && params == NULL);
    xmlrpc_env_clean(&env2);

    /* Next, check for bogus values. These are all well-formed XML, but
    ** they aren't legal XML-RPC. */
    for (bad_call = bad_calls; *bad_call != NULL; bad_call++) {
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        elem = xml_parse(&env, *bad_call, strlen(*bad_call));
        TEST_NO_FAULT(&env);
        xml_element_free(elem);

        /* Now, make sure the higher-level routine barfs appropriately. */
        xmlrpc_env_init(&env2);
        xmlrpc_parse_call(&env2, *bad_call, strlen(*bad_call),
                          &method_name, &params);
        TEST_FAULT(&env2, XMLRPC_PARSE_ERROR);
        TEST(method_name == NULL && params == NULL);
        xmlrpc_env_clean(&env2);
    }

    xmlrpc_env_clean(&env);    
}



void
test_parse_xml(void) {

    test_expat();
    test_parse_xml_value();
    test_parse_xml_response();
    test_parse_xml_call();
}
