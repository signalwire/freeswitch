#include <limits.h>
#include <stdlib.h>

#include "xmlrpc_config.h"

#include "girstring.h"
#include "casprintf.h"
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
    xml_parse(&env, expat_data, strlen(expat_data), &elem);
    TEST_NO_FAULT(&env);
    TEST(elem != NULL);

    /* Verify our results. */
    TEST(streq(xml_element_name(elem), "value"));
    TEST(xml_element_children_size(elem) == 1);
    array = xml_element_children(elem)[0];
    TEST(streq(xml_element_name(array), "array"));
    TEST(xml_element_children_size(array) == 1);
    data = xml_element_children(array)[0];
    TEST(streq(xml_element_name(data), "data"));
    TEST(xml_element_children_size(data) > 1);
    value1 = xml_element_children(data)[0];
    TEST(streq(xml_element_name(value1), "value"));
    TEST(xml_element_children_size(value1) == 1);
    i4 = xml_element_children(value1)[0];
    TEST(streq(xml_element_name(i4), "i4"));
    TEST(xml_element_children_size(i4) == 0);
    cdata = xml_element_cdata(i4);
    size = xml_element_cdata_size(i4);
    TEST(size == strlen("2147483647"));
    TEST(memcmp(cdata, "2147483647", strlen("2147483647")) == 0);

    /* Test cleanup code (w/memprof). */
    xml_element_free(elem);

    /* Test broken XML */
    xml_parse(&env, expat_error_data, strlen(expat_error_data), &elem);
    TEST(env.fault_occurred);

    xmlrpc_env_clean(&env);
}



static void
testParseNumberValue(void) {

char const xmldata[] =
    XML_PROLOGUE
    "<methodCall>\r\n"
    "<methodName>test</methodName>\r\n"
    "<params>\r\n"
    "<param><value><int>2147483647</int></value></param>\r\n" \
    "<param><value><int>-2147483648</int></value></param>\r\n" \
    "<param><value><i1>10</i1></value></param>\r\n"
    "<param><value><i2>10</i2></value></param>\r\n"
    "<param><value><i4>10</i4></value></param>\r\n"
    "<param><value><i8>10</i8></value></param>\r\n"
    "<param><value><ex.i8>10</ex.i8></value></param>\r\n"
    "<param><value><double>10</double></value></param>\r\n"
    "<param><value><double>10.1</double></value></param>\r\n"
    "<param><value><double>-10.1</double></value></param>\r\n"
    "<param><value><double>+10.1</double></value></param>\r\n"
    "<param><value><double>0</double></value></param>\r\n"
    "<param><value><double>.01</double></value></param>\r\n"
    "<param><value><double>5.</double></value></param>\r\n"
    "<param><value><double>5.3E6</double></value></param>\r\n"
    "<param><value><double> 1</double></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n";
    
    xmlrpc_env env;
    xmlrpc_value * paramArrayP;
    const char * methodName;
    int arraySize;
    xmlrpc_int int_max, int_min;
    xmlrpc_int32 i_i1, i_i2, i_i4;
    xmlrpc_int64 i_i8, i_ex_i8;
    double d1, d2, d3, d4, d5, d6, d7, d8, d9;

    xmlrpc_env_init(&env);

    xmlrpc_parse_call(&env, xmldata, strlen(xmldata),
                      &methodName, &paramArrayP);
    TEST_NO_FAULT(&env);

    arraySize = xmlrpc_array_size(&env, paramArrayP);
    TEST_NO_FAULT(&env);

    TEST(arraySize == 16);

    xmlrpc_decompose_value(
        &env, paramArrayP, "(iiiiiIIddddddddd)", 
        &int_max, &int_min, &i_i1, &i_i2, &i_i4, &i_i8, &i_ex_i8,
        &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9);

    TEST_NO_FAULT(&env);

    TEST(int_max == INT_MAX);
    TEST(int_min == INT_MIN);
    TEST(i_i1 == 10);
    TEST(i_i2 == 10);
    TEST(i_i4 == 10);
    TEST(i_i8 == 10);
    TEST(i_ex_i8 == 10);
    TESTFLOATEQUAL(d1, 10.0);
    TESTFLOATEQUAL(d2, 10.1);
    TESTFLOATEQUAL(d3, -10.1);
    TESTFLOATEQUAL(d4, +10.1);
    TESTFLOATEQUAL(d5, 0.0);
    TESTFLOATEQUAL(d6, 0.01);
    TESTFLOATEQUAL(d7, 5.0);
    TESTFLOATEQUAL(d8, 5.3E6);
    TESTFLOATEQUAL(d9, 1.0);

    xmlrpc_DECREF(paramArrayP);
    strfree(methodName);

    xmlrpc_env_clean(&env);
}



static void
testParseMiscSimpleValue(void) {

char const xmldata[] =
    XML_PROLOGUE
    "<methodCall>\r\n"
    "<methodName>test</methodName>\r\n"
    "<params>\r\n"
    "<param><value><string>hello</string></value></param>\r\n"
    "<param><value><boolean>0</boolean></value></param>\r\n"
    "<param><value><boolean>1</boolean></value></param>\r\n"
    "<param><value><dateTime.iso8601>19980717T14:08:55</dateTime.iso8601>"
       "</value></param>\r\n"
    "<param><value><base64>YmFzZTY0IGRhdGE=</base64></value></param>\r\n"
    "<param><value><nil/></value></param>\r\n"
    "<param><value><ex.nil/></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n";
    
    xmlrpc_env env;
    xmlrpc_value * paramArrayP;
    const char * methodName;
    int arraySize;
    const char * str_hello;
    xmlrpc_bool b_false, b_true;
    const char * datetime;
    unsigned char * b64_data;
    size_t b64_len;

    xmlrpc_env_init(&env);

    xmlrpc_parse_call(&env, xmldata, strlen(xmldata),
                      &methodName, &paramArrayP);
    TEST_NO_FAULT(&env);

    arraySize = xmlrpc_array_size(&env, paramArrayP);
    TEST_NO_FAULT(&env);

    TEST(arraySize == 7);

    xmlrpc_decompose_value(
        &env, paramArrayP, "(sbb86nn)", 
        &str_hello, &b_false, &b_true, &datetime, &b64_data, &b64_len);

    TEST_NO_FAULT(&env);

    TEST(streq(str_hello, "hello"));
    TEST(!b_false);
    TEST(b_true);
    TEST(streq(datetime, "19980717T14:08:55")); 
    TEST(b64_len == 11);
    TEST(memcmp(b64_data, "base64 data", b64_len) == 0);

    free(b64_data);
    strfree(str_hello);
    strfree(datetime);
    xmlrpc_DECREF(paramArrayP);
    strfree(methodName);

    xmlrpc_env_clean(&env);
}



static void
validateParseResponseResult(xmlrpc_value * const valueP) {

    xmlrpc_env env;

    xmlrpc_value * s;
    xmlrpc_int32 int_max;
    xmlrpc_int32 int_min;
    xmlrpc_int32 int_one;
    xmlrpc_bool bool_false;
    xmlrpc_bool bool_true;
    char * str_hello;
    char * str_untagged;
    char * datetime;
    unsigned char * b64_data;
    size_t b64_len;
    double negone;
    double zero;
    double one;

    xmlrpc_env_init(&env);

    xmlrpc_decompose_value(
        &env, valueP, "((iibbs68())idddSs)", 
        &int_max, &int_min,
        &bool_false, &bool_true, &str_hello,
        &b64_data, &b64_len, &datetime,
        &int_one, &negone, &zero, &one, &s, &str_untagged);

    TEST_NO_FAULT(&env);
    TEST(int_max == INT_MAX);
    TEST(int_min == INT_MIN);
    TEST(!bool_false);
    TEST(bool_true);
    TEST(strlen(str_hello) == strlen("Hello, world! <&>"));
    TEST(streq(str_hello, "Hello, world! <&>"));
    TEST(b64_len == 11);
    TEST(memcmp(b64_data, "base64 data", b64_len) == 0);
    TEST(streq(datetime, "19980717T14:08:55"));
    TEST(int_one == 1);
    TEST(negone == -1.0);
    TEST(zero == 0.0);
    TEST(one == 1.0);
    TEST(streq(str_untagged, "Untagged string"));
    free(str_hello);
    free(b64_data);
    free(datetime);
    free(str_untagged);

    {
        /* Analyze the contents of our struct. */

        xmlrpc_value * sval;
        int size, sval_int;

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
    }    

    xmlrpc_env_clean(&env);
}



static void
testParseGoodResponse(void) {

    xmlrpc_env env;
    xmlrpc_value * valueP;
    int faultCode;
    const char * faultString;

    xmlrpc_env_init(&env);

    xmlrpc_parse_response2(&env, good_response_xml, strlen(good_response_xml),
                           &valueP, &faultCode, &faultString);

    TEST_NO_FAULT(&env);
    TEST(faultString == NULL);

    validateParseResponseResult(valueP);

    xmlrpc_DECREF(valueP);

    /* Try it again with old interface */

    valueP = xmlrpc_parse_response(&env,
                                   good_response_xml,
                                   strlen(good_response_xml));
    TEST_NO_FAULT(&env);
    TEST(valueP != NULL);

    validateParseResponseResult(valueP);

    xmlrpc_DECREF(valueP);

    xmlrpc_env_clean(&env);

}    



static void
testParseBadResponseXml(void) {
/*----------------------------------------------------------------------------
   Test parsing of data that is supposed to be a response, but in not
   even valid XML.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_value * valueP;
    int faultCode;
    const char * faultString;

    xmlrpc_env_init(&env);
       
    xmlrpc_parse_response2(&env,
                           unparseable_value, strlen(unparseable_value),
                           &valueP, &faultCode, &faultString);
        
    TEST_FAULT(&env, XMLRPC_PARSE_ERROR);
    xmlrpc_env_clean(&env);

    xmlrpc_env_init(&env);

        /* And again with the old interface */
    valueP = xmlrpc_parse_response(&env, unparseable_value,
                                   strlen(unparseable_value));
    TEST_FAULT(&env, XMLRPC_PARSE_ERROR);
    xmlrpc_env_clean(&env);
    TEST(valueP == NULL);
}



static void
testParseBadResponseXmlRpc(void) {
/*----------------------------------------------------------------------------
   Test parsing of data that is supposed to be a response, and is valid
   XML, but is not valid XML-RPC.
-----------------------------------------------------------------------------*/
    unsigned int i;

    /* For this test, we test up to but not including the <value> in a
       successful RPC response. 
    */

    /* Next, check for bogus responses. These are all well-formed XML, but
    ** they aren't legal XML-RPC. */
    for (i = 15; bad_responses[i] != NULL; ++i) {
        const char * const bad_resp = bad_responses[i];
        xmlrpc_env env;
        xmlrpc_value * v;
        xml_element *elem;

        xmlrpc_env_init(&env);
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        xml_parse(&env, bad_resp, strlen(bad_resp), &elem);
        TEST_NO_FAULT(&env);
        xml_element_free(elem);
    
        /* Now, make sure the higher-level routine barfs appropriately. */
        v = xmlrpc_parse_response(&env, bad_resp, strlen(bad_resp));
        TEST(env.fault_occurred);
        TEST(env.fault_code != 0); /* We use 0 as a code in our bad faults. */
        TEST(v == NULL);
        xmlrpc_env_clean(&env);
    }
}    



static void
testParseBadResult(void) {
/*----------------------------------------------------------------------------
   Test parsing of data that is supposed to be a response, but is not
   valid.  It looks like a valid success response, but the result value
   is not valid XML-RPC.
-----------------------------------------------------------------------------*/
    unsigned int i;

    for (i = 0; bad_values[i] != NULL; ++i) {
        const char * const bad_resp = bad_values[i];
        xmlrpc_env env;
        xmlrpc_value * valueP;
        xml_element *elem;
        int faultCode;
        const char * faultString;
    
        xmlrpc_env_init(&env);

        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        xml_parse(&env, bad_resp, strlen(bad_resp), &elem);
        TEST_NO_FAULT(&env);
        xml_element_free(elem);
    
        /* Now, make sure the higher-level routine barfs appropriately. */
        
        xmlrpc_parse_response2(&env, bad_resp, strlen(bad_resp),
                               &valueP, &faultCode, &faultString);

        TEST_FAULT(&env, XMLRPC_PARSE_ERROR);
        xmlrpc_env_clean(&env);

        xmlrpc_env_init(&env);

        /* And again with the old interface */

        valueP = xmlrpc_parse_response(&env, bad_resp, strlen(bad_resp));
        TEST_FAULT(&env, XMLRPC_PARSE_ERROR);
        TEST(valueP == NULL);
        xmlrpc_env_clean(&env);
    }
}



static void
testParseBadResponse(void) {
/*----------------------------------------------------------------------------
   Test parsing of data that is supposed to be a response, but is not
   valid.  Either not valid XML or not valid XML-RPC.
-----------------------------------------------------------------------------*/
    testParseBadResponseXml();

    testParseBadResponseXmlRpc();

    testParseBadResult();
}



static void
testParseFaultResponse(void) {
/*----------------------------------------------------------------------------
   Test parsing of a valid response that indicates the RPC failed.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    {
        xmlrpc_value * resultP;
        int faultCode;
        const char * faultString;
            
        xmlrpc_parse_response2(&env,
                               serialized_fault, strlen(serialized_fault),
                               &resultP, &faultCode, &faultString);

        TEST_NO_FAULT(&env);
        TEST(faultString != NULL);
        TEST(faultCode == 6);
        TEST(streq(faultString, "A fault occurred"));
        strfree(faultString);
    }
    /* Now with the old interface */
    {
        xmlrpc_value * valueP;
        xmlrpc_env fault;

        /* Parse a valid fault. */
        xmlrpc_env_init(&fault);
        valueP = xmlrpc_parse_response(&fault, serialized_fault,
                                       strlen(serialized_fault));

        TEST(fault.fault_occurred);
        TEST(fault.fault_code == 6);
        TEST(streq(fault.fault_string, "A fault occurred"));
        xmlrpc_env_clean(&fault);
    }

    xmlrpc_env_clean(&env);
}



static void
test_parse_xml_call(void) {

    xmlrpc_env env;
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
    TEST(streq(method_name, "gloom&doom"));
    TEST(i1 == 10 && i2 == 20);
    strfree(method_name);

    /* Test some poorly-formed XML data. */
    xmlrpc_parse_call(&env, unparseable_value, strlen(unparseable_value),
                      &method_name, &params);
    TEST_FAULT(&env, XMLRPC_PARSE_ERROR);
    TEST(method_name == NULL && params == NULL);

    /* Next, check for bogus values. These are all well-formed XML, but
       they aren't legal XML-RPC.
    */
    for (bad_call = bad_calls; *bad_call != NULL; ++bad_call) {
    
        /* First, check to make sure that our test case is well-formed XML.
        ** (It's easy to make mistakes when writing the test cases!) */
        xml_parse(&env, *bad_call, strlen(*bad_call), &elem);
        TEST_NO_FAULT(&env);
        xml_element_free(elem);

        /* Now, make sure the higher-level routine barfs appropriately. */
        xmlrpc_parse_call(&env, *bad_call, strlen(*bad_call),
                          &method_name, &params);
        TEST_FAULT(&env, XMLRPC_PARSE_ERROR);
        TEST(method_name == NULL && params == NULL);
    }
    xmlrpc_env_clean(&env);    
}



void
test_parse_xml(void) {

    printf("Running XML parsing tests.\n");
    test_expat();
    testParseNumberValue();
    testParseMiscSimpleValue();
    testParseGoodResponse();
    testParseFaultResponse();
    testParseBadResponse();
    test_parse_xml_call();
    printf("\n");
    printf("XML parsing tests done.\n");
}
