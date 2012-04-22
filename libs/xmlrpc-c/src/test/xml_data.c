#include <stddef.h>

#include "xml_data.h"

#define RAW_STRING_DATA \
    "<value><array><data>\r\n" \
    "<value><i4>2147483647</i4></value>\r\n" \
    "<value><i4>-2147483648</i4></value>\r\n" \
    "<value><boolean>0</boolean></value>\r\n" \
    "<value><boolean>1</boolean></value>\r\n" \
    "<value><string>Hello, world! &lt;&amp;&gt;</string></value>\r\n" \
    "<value><base64>\r\n" \
    "YmFzZTY0IGRhdGE=\r\n" \
    "</base64></value>\r\n" \
    "<value>" \
      "<dateTime.iso8601>19980717T14:08:55</dateTime.iso8601>" \
      "</value>\r\n" \
    "<value><array><data>\r\n" \
    "</data></array></value>\r\n" \
    "</data></array></value>"
    
char const serialized_data[] = RAW_STRING_DATA;

char const serialized_call[] =
    XML_PROLOGUE
    "<methodCall>\r\n"
    "<methodName>gloom&amp;doom</methodName>\r\n"
    "<params>\r\n"
    "<param><value><i4>10</i4></value></param>\r\n"
    "<param><value><i4>20</i4></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n";

char const serialized_fault[] =
    XML_PROLOGUE
    "<methodResponse>\r\n"
    "<fault>\r\n"
    "<value><struct>\r\n"
    "<member><name>faultCode</name>\r\n"
    "<value><i4>6</i4></value></member>\r\n"
    "<member><name>faultString</name>\r\n"
    "<value><string>A fault occurred</string></value></member>\r\n"
    "</struct></value>\r\n"
    "</fault>\r\n"
    "</methodResponse>\r\n";

char const expat_data[] = XML_PROLOGUE RAW_STRING_DATA "\r\n";
char const expat_error_data[] =
    XML_PROLOGUE \
    "<foo><bar>abc</bar><baz></baz>\r\n";
    /* Invalid because there's no closing </foo> */


char const good_response_xml[] = 
    XML_PROLOGUE 
    "<methodResponse><params><param>\r\n" 
    "<value><array><data>\r\n" 
    RAW_STRING_DATA "\r\n" 
    "<value><int>1</int></value>\r\n" 
    "<value><double>-1.0</double></value>\r\n" 
    "<value><double>0.0</double></value>\r\n" 
    "<value><double>1.0</double></value>\r\n" 
    "<value><struct>\r\n" 
    "<member><name>ten &lt;&amp;&gt;</name>\r\n" 
    "<value><i4>10</i4></value></member>\r\n" 
    "<member><name>twenty</name>\r\n" 
    "<value><i4>20</i4></value></member>\r\n" 
    "</struct></value>\r\n" 
    "<value>Untagged string</value>\r\n" 
    "</data></array></value>\r\n" 
    "</param></params></methodResponse>\r\n";

#define VALUE_HEADER \
    XML_PROLOGUE"<methodResponse><params><param><value>\r\n"
#define VALUE_FOOTER \
    "</value></param></params></methodResponse>\r\n"

#define MEMBER_HEADER \
    VALUE_HEADER"<struct><member>"
#define MEMBER_FOOTER \
    "</member></struct>"VALUE_FOOTER
#define ARBITRARY_VALUE \
    "<value><i4>0</i4></value>"

char const unparseable_value[] = VALUE_HEADER"<i4>"VALUE_FOOTER;

const char * bad_values[] = {
    VALUE_HEADER"<i4>0</i4><i4>0</i4>"VALUE_FOOTER,
    VALUE_HEADER"<foo></foo>"VALUE_FOOTER,
    VALUE_HEADER"<i4><i4>4</i4></i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4>2147483648</i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4>-2147483649</i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4> 0</i4>"VALUE_FOOTER,
    VALUE_HEADER"<i4>0 </i4>"VALUE_FOOTER,
    VALUE_HEADER"<boolean>2</boolean>"VALUE_FOOTER,
    VALUE_HEADER"<boolean>-1</boolean>"VALUE_FOOTER,
    VALUE_HEADER"<double></double>"VALUE_FOOTER,
    VALUE_HEADER"<double>0.0 </double>"VALUE_FOOTER,
    VALUE_HEADER"<double>a</double>"VALUE_FOOTER,
    VALUE_HEADER"<double>1.1.1</double>"VALUE_FOOTER,
    VALUE_HEADER"<double>1a</double>"VALUE_FOOTER,
    VALUE_HEADER"<double>1.1a</double>"VALUE_FOOTER,
    VALUE_HEADER"<array></array>"VALUE_FOOTER,
    VALUE_HEADER"<array><data></data><data></data></array>"VALUE_FOOTER,
    VALUE_HEADER"<array><data></data><data></data></array>"VALUE_FOOTER,
    VALUE_HEADER"<array><data><foo></foo></data></array>"VALUE_FOOTER,
    VALUE_HEADER"<struct><foo></foo></struct>"VALUE_FOOTER,
    MEMBER_HEADER MEMBER_FOOTER,
    MEMBER_HEADER"<name>a</name>"MEMBER_FOOTER,
    MEMBER_HEADER"<name>a</name>"ARBITRARY_VALUE"<f></f>"MEMBER_FOOTER,
    MEMBER_HEADER"<foo></foo>"ARBITRARY_VALUE MEMBER_FOOTER,
    MEMBER_HEADER"<name>a</name><foo></foo>"MEMBER_FOOTER,
    MEMBER_HEADER"<name><foo></foo></name>"ARBITRARY_VALUE MEMBER_FOOTER,
    NULL
};

#define RESPONSE_HEADER \
    XML_PROLOGUE"<methodResponse>\r\n"
#define RESPONSE_FOOTER \
    "</methodResponse>\r\n"

#define PARAMS_RESP_HEADER \
    RESPONSE_HEADER"<params>"
#define PARAMS_RESP_FOOTER \
    "</params>"RESPONSE_FOOTER

#define FAULT_HEADER \
    RESPONSE_HEADER"<fault>"
#define FAULT_FOOTER \
    "</fault>"RESPONSE_FOOTER

#define FAULT_STRUCT_HEADER \
    FAULT_HEADER"<value><struct>"
#define FAULT_STRUCT_FOOTER \
    "</struct></value>"FAULT_FOOTER

const char * bad_responses[] = {
    XML_PROLOGUE"<foo></foo>\r\n",
    RESPONSE_HEADER RESPONSE_FOOTER,
    RESPONSE_HEADER"<params></params><params></params>"RESPONSE_FOOTER,
    RESPONSE_HEADER"<foo></foo>"RESPONSE_FOOTER,

    /* Make sure we insist on only one parameter in a response. */
    PARAMS_RESP_HEADER PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER
    "<param><i4>0</i4></param>"
    "<param><i4>0</i4></param>"
    PARAMS_RESP_FOOTER,

    /* Test other sorts of bad parameters. */
    PARAMS_RESP_HEADER"<foo></foo>"PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER"<param></param>"PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER"<param><foo></foo></param>"PARAMS_RESP_FOOTER,
    PARAMS_RESP_HEADER
    "<param>"ARBITRARY_VALUE ARBITRARY_VALUE"</param>"
    PARAMS_RESP_FOOTER,
    
    /* Basic fault tests. */
    FAULT_HEADER FAULT_FOOTER,
    FAULT_HEADER"<foo></foo>"FAULT_FOOTER,
    FAULT_HEADER"<value></value><value></value>"FAULT_FOOTER,
    FAULT_HEADER"<value><i4>1</i4></value>"FAULT_FOOTER,

    /* Make sure we insist on the proper members within the fault struct. */
    FAULT_STRUCT_HEADER
    "<member><name>faultString</name>"
    "<value><string>foo</string></value></member>"
    FAULT_STRUCT_FOOTER,
    FAULT_STRUCT_HEADER
    "<member><name>faultCode</name>"
    "<value><i4>0</i4></value></member>"
    FAULT_STRUCT_FOOTER,
    FAULT_STRUCT_HEADER
    "<member><name>faultCode</name>"
    "<value><i4>0</i4></value></member>"
    "<member><name>faultString</name>"
    "<value><i4>0</i4></value></member>"
    FAULT_STRUCT_FOOTER,
    FAULT_STRUCT_HEADER
    "<member><name>faultCode</name>"
    "<value><string>0</string></value></member>"
    "<member><name>faultString</name>"
    "<value><string>foo</string></value></member>"
    FAULT_STRUCT_FOOTER,
    NULL};

#define CALL_HEADER \
    XML_PROLOGUE"<methodCall>\r\n"
#define CALL_FOOTER \
    "</methodCall>\r\n"

const char * bad_calls[] = {
    XML_PROLOGUE"<foo></foo>\r\n",
    CALL_HEADER CALL_FOOTER,
    CALL_HEADER"<methodName>m</methodName><foo></foo>"CALL_FOOTER, 
    CALL_HEADER"<foo></foo><params></params>"CALL_FOOTER, 
    CALL_HEADER"<methodName><f></f></methodName><params></params>"CALL_FOOTER, 
    NULL};


