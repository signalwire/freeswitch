#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <memory>
#include <time.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "transport_config.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/oldcppwrapper.hpp"
#include "xmlrpc-c/registry.hpp"

#include "testclient.hpp"
#include "registry.hpp"
#include "server_abyss.hpp"
#include "server_pstream.hpp"
#include "tools.hpp"

using namespace xmlrpc_c;
using namespace std;

//=========================================================================
//  Test Harness
//=========================================================================
// 
//  There are two styles of test in here.  The older ones are vaguely
//  inspired by Kent Beck's book on eXtreme Programming (XP) and use
//  the TEST...() macros.
//
//  But this style is not really appropriate for C++.  It's based on
//  code that explicitly tests for errors, as one would do in C.  In C++,
//  it is cumbersome to catch exceptions on every call, so we don't in
//  the new style.

//  And there's not much point in trying to count test successes and
//  failures.  Any failure is a problem, so in the new style, we just
//  quit after we recognize one (again, more in line with regular exception
//  throwing).  With exception throwing, you can't count what _didn't_
//  cause an exception, so there's no meaningful count of test successes.
//
//  To run the tests, type './cpptest'.
//  To check for memory leaks, install RedHat's 'memprof' utility, and
//  type 'memprof cpptest'.
//
//  If you add new tests to this file, please deallocate any data
//  structures you use in the appropriate fashion. This allows us to test
//  various destructor code for memory leaks.


//=========================================================================
//  Test Suites
//=========================================================================

void 
test_fault (void) {

    // Create a new fault and perform basic operations.
    XmlRpcFault fault1 = XmlRpcFault(6, "Sample fault");
    TEST(fault1.getFaultCode() == 6);
    TEST(fault1.getFaultString() == "Sample fault");

    // Extract and examine the underlying xmlrpc_env struct.
    xmlrpc_env *env1 = fault1.getFaultEnv();
    TEST(env1 != NULL);
    TEST(env1->fault_occurred);
    TEST(env1->fault_code == 6);
    TEST(strcmp(env1->fault_string, "Sample fault") == 0);

    // Test our copy constructor.
    XmlRpcFault fault2 = fault1;
    TEST(fault2.getFaultCode() == 6);
    TEST(fault2.getFaultString() == "Sample fault");
    
    // Construct a fault from a pre-existing xmlrpc_env structure.
    xmlrpc_env env3;
    xmlrpc_env_init(&env3);
    xmlrpc_env_set_fault(&env3, 7, "Another fault");
    XmlRpcFault fault3 = XmlRpcFault(&env3);
    xmlrpc_env_clean(&env3);
    TEST(fault3.getFaultCode() == 7);
    TEST(fault3.getFaultString() == "Another fault");
    
    // Attempt to construct a fault from a fault-free xmlrpc_env.
    xmlrpc_env env4;
    xmlrpc_env_init(&env4);
    try {
        XmlRpcFault fault4 = XmlRpcFault(&env4);
        TEST_FAILED("Constructed invalid XmlRpcFault");
    } catch (XmlRpcFault const& fault) {
        TEST_PASSED();
        TEST(fault.getFaultCode() == XMLRPC_INTERNAL_ERROR);
    }
    xmlrpc_env_clean(&env4);
}



void test_env (void) {

    // Declare these here to prevent silly compiler warnings about
    // potentially uninitialized variables.
    XmlRpcEnv env1;
    XmlRpcEnv env2;

    // Perform simple environment tests.
    TEST(!env1.hasFaultOccurred());
    xmlrpc_env_set_fault(env1, 8, "Fault 8");
    TEST(env1.hasFaultOccurred());
    XmlRpcFault fault1 = env1.getFault();
    TEST(fault1.getFaultCode() == 8);
    TEST(fault1.getFaultString() == "Fault 8");

    // Test throwIfFaultOccurred.
    try {
        env2.throwIfFaultOccurred();
        TEST_PASSED();
    } catch (XmlRpcFault const&) {
        TEST_FAILED("We threw a fault when one hadn't occurred");
    } 
    xmlrpc_env_set_fault(env2, 9, "Fault 9");
    try {
        env2.throwIfFaultOccurred();
        TEST_FAILED("A fault occurred, and we didn't throw it");
    } catch (XmlRpcFault const& fault) {
        TEST_PASSED();
        TEST(fault.getFaultCode() == 9);
        TEST(fault.getFaultString() == "Fault 9");
    } 
    
    // Make sure we can't get a fault if one hasn't occurred.
    XmlRpcEnv env3;
    try {
        XmlRpcFault fault3 = env3.getFault();
        TEST_FAILED("We retrieved a non-existant fault");
    } catch (XmlRpcFault const& fault) {
        TEST_PASSED();
        TEST(fault.getFaultCode() == XMLRPC_INTERNAL_ERROR);
    }
}

void test_value (void) {
    XmlRpcEnv env;

    // Test basic reference counting behavior.
    xmlrpc_value *v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 1);
    env.throwIfFaultOccurred();
    XmlRpcValue val1 = XmlRpcValue(v, XmlRpcValue::CONSUME_REFERENCE);
    v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 2);
    env.throwIfFaultOccurred();
    XmlRpcValue val2 = v;
    xmlrpc_DECREF(v);

    // Borrow a reference.
    v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 3);
    env.throwIfFaultOccurred();
    XmlRpcValue val3 = XmlRpcValue(v, XmlRpcValue::CONSUME_REFERENCE);
    xmlrpc_value *borrowed = val3.borrowReference();
    TEST(borrowed == v);

    // Make a reference.
    v = xmlrpc_build_value(env, "i", (xmlrpc_int32) 4);
    env.throwIfFaultOccurred();
    XmlRpcValue val4 = XmlRpcValue(v, XmlRpcValue::CONSUME_REFERENCE);
    xmlrpc_value *made = val4.makeReference();
    TEST(made == v);
    xmlrpc_DECREF(made);

    // Test our default constructor.
    XmlRpcValue val5;
    TEST(val5.getBool() == false);

    // Test our type introspection.
    TEST(XmlRpcValue::makeInt(0).getType() == XMLRPC_TYPE_INT);
    
    // Test our basic data types.
    TEST(XmlRpcValue::makeInt(30).getInt() == 30);
    TEST(XmlRpcValue::makeInt(-30).getInt() == -30);
    TEST(XmlRpcValue::makeBool(true).getBool() == true);
    TEST(XmlRpcValue::makeBool(false).getBool() == false);
    TEST(XmlRpcValue::makeDateTime("19980717T14:08:55").getRawDateTime() ==
         "19980717T14:08:55");
    TEST(XmlRpcValue::makeString("foo").getString() == "foo");
    TEST(XmlRpcValue::makeString("bar", 3).getString() == "bar");
    TEST(XmlRpcValue::makeString("bar", 3).getString() == "bar");
    TEST(XmlRpcValue::makeString("a\0b").getString() == string("a\0b"));
    XmlRpcValue::makeArray().getArray();
    XmlRpcValue::makeStruct().getStruct();

    // Test Base64 values.
    const unsigned char *b64_data;
    size_t b64_len;
    XmlRpcValue val6 = XmlRpcValue::makeBase64((unsigned char*) "a\0\0b", 4);
    val6.getBase64(b64_data, b64_len);
    TEST(b64_len == 4);
    TEST(memcmp(b64_data, "a\0\0b", 4) == 0);

    // Test arrays.
    XmlRpcValue array = XmlRpcValue::makeArray();
    TEST(array.arraySize() == 0);
    array.arrayAppendItem(XmlRpcValue::makeString("foo"));
    TEST(array.arraySize() == 1);
    array.arrayAppendItem(XmlRpcValue::makeString("bar"));
    TEST(array.arraySize() == 2);
    TEST(array.arrayGetItem(0).getString() == "foo");
    TEST(array.arrayGetItem(1).getString() == "bar");

    // Test structs.
    XmlRpcValue strct = XmlRpcValue::makeStruct();
    TEST(strct.structSize() == 0);
    strct.structSetValue("foo", XmlRpcValue::makeString("fooval"));
    TEST(strct.structSize() == 1);
    strct.structSetValue("bar", XmlRpcValue::makeString("barval"));
    TEST(strct.structSize() == 2);
    TEST(strct.structHasKey("bar"));
    TEST(!strct.structHasKey("nosuch"));
    for (size_t i = 0; i < strct.structSize(); i++) {
        string key;
        XmlRpcValue value;
        strct.structGetKeyAndValue(i, key, value);
        TEST(key + "val" == value.getString());
    }
}



static void
testXmlRpcCpp() {
/*----------------------------------------------------------------------------
   Test the legacy XmlRpcCpp.cpp library
-----------------------------------------------------------------------------*/
    cout << "Testing XmlRpcCpp library..." << endl;

    test_fault();
    test_env();
    test_value();
}



class intTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "intTestSuite";
    }
    virtual void runtests(unsigned int const) {
        value_int int1(7);
        TEST(static_cast<int>(int1) == 7);
        value_int int2(-7);
        TEST(static_cast<int>(int2) == -7);
        value val1(int1);
        TEST(val1.type() == value::TYPE_INT);
        value_int int3(val1);
        TEST(static_cast<int>(int3) == 7);
        try {
            value_int int4(value_double(3.7));
            TEST_FAILED("invalid cast double-int suceeded");
        } catch (error) {}
    }
};



class doubleTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "doubleTestSuite";
    }
    virtual void runtests(unsigned int const) {
        value_double double1(3.14);
        TEST(static_cast<double>(double1) == 3.14);
        value val1(double1);
        TEST(val1.type() == value::TYPE_DOUBLE);
        value_double double2(val1);
        TEST(static_cast<double>(double2) == 3.14);
        try {
            value_double double4(value_int(4));
            TEST_FAILED("invalid cast int-double suceeded");
        } catch (error) {}
    }
};



class booleanTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "booleanTestSuite";
    }
    virtual void runtests(unsigned int const) {
        value_boolean boolean1(true); 
        TEST(static_cast<bool>(boolean1) == true);
        value_boolean boolean2(false);
        TEST(static_cast<bool>(boolean2) == false);
        value val1(boolean1);
        TEST(val1.type() == value::TYPE_BOOLEAN);
        value_boolean boolean3(val1);
        TEST(static_cast<bool>(boolean3) == true);
        try {
            value_boolean boolean4(value_int(4));
            TEST_FAILED("invalid cast int-boolean suceeded");
        } catch (error) {}
    }
};



class datetimeTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "datetimeTestSuite";
    }
    virtual void runtests(unsigned int const) {
        time_t const testTime(900684535);
        value_datetime datetime1("19980717T14:08:55");
        TEST(static_cast<time_t>(datetime1) == testTime);
        value_datetime datetime2(testTime);
        TEST(static_cast<time_t>(datetime2) == testTime);
        value val1(datetime1);
        TEST(val1.type() == value::TYPE_DATETIME);
        value_datetime datetime3(val1);
        TEST(static_cast<time_t>(datetime3) == testTime);
        try {
            value_datetime datetime4(value_int(4));
            TEST_FAILED("invalid cast int-datetime suceeded");
        } catch (error) {}
    }
};



class stringTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "stringTestSuite";
    }
    virtual void runtests(unsigned int const) {
        value_string string1("hello world");
        TEST(static_cast<string>(string1) == "hello world");
        value_string string2("embedded\0null");
        TEST(static_cast<string>(string2) == "embedded\0null");
        value val1(string1);
        TEST(val1.type() == value::TYPE_STRING);
        value_string string3(val1);
        TEST(static_cast<string>(string3) == "hello world");
        try {
            value_string string4(value_int(4));
            TEST_FAILED("invalid cast int-string succeeded");
        } catch (error) {}
        value_string string5("hello world", value_string::nlCode_all);
        TEST(static_cast<string>(string5) == "hello world");
        value_string string6("hello\nthere\rworld\r\n\n",
                             value_string::nlCode_all);
        TEST(static_cast<string>(string6) == "hello\nthere\nworld\n\n");
        TEST(string6.crlfValue() == "hello\r\nthere\r\nworld\r\n\r\n");
        value_string string7("hello\nthere\rworld\r\n\n",
                             value_string::nlCode_lf);
        TEST(static_cast<string>(string7) == "hello\nthere\rworld\r\n\n");
    }
};



class bytestringTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "bytestringTestSuite";
    }
    virtual void runtests(unsigned int const) {
        unsigned char bytestringArray[] = {0x10, 0x11, 0x12, 0x13, 0x14};
        vector<unsigned char> 
            bytestringData(&bytestringArray[0], &bytestringArray[4]);
        value_bytestring bytestring1(bytestringData);

        vector<unsigned char> const dataReadBack1(
            bytestring1.vectorUcharValue());
        TEST(dataReadBack1 == bytestringData);
        value val1(bytestring1);
        TEST(val1.type() == value::TYPE_BYTESTRING);
        value_bytestring bytestring2(val1);
        vector<unsigned char> const dataReadBack2(
            bytestring2.vectorUcharValue());
        TEST(dataReadBack2 == bytestringData);
        try {
            value_bytestring bytestring4(value_int(4));
            TEST_FAILED("invalid cast int-bytestring suceeded");
        } catch (error) {}
    }
};



class nilTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "nilTestSuite";
    }
    virtual void runtests(unsigned int const) {
        value_nil nil1;
        value val1(nil1);
        TEST(val1.type() == value::TYPE_NIL);
        value_nil nil2(val1);
        try {
            value_nil nil4(value_int(4));
            TEST_FAILED("invalid cast int-nil suceeded");
        } catch (error) {}
    }
};



class i8TestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "i8TestSuite";
    }
    virtual void runtests(unsigned int const) {
        value_i8 int1(7);
        TEST(static_cast<xmlrpc_int64>(int1) == 7);
        value_i8 int2(-7);
        TEST(static_cast<xmlrpc_int64>(int2) == -7);
        value_i8 int5(1ull << 40);
        TEST(static_cast<xmlrpc_int64>(int5) == (1ull << 40));
        value val1(int1);
        TEST(val1.type() == value::TYPE_I8);
        value_i8 int3(val1);
        TEST(static_cast<xmlrpc_int64>(int3) == 7);
        try {
            value_i8 int4(value_double(3.7));
            TEST_FAILED("invalid cast double-i8 suceeded");
        } catch (error) {}
    }
};



class structTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "structTestSuite";
    }
    virtual void runtests(unsigned int const) {
        map<string, value> structData;
        pair<string, value> member("the_integer", value_int(9));
        structData.insert(member);
        
        value_struct struct1(structData);

        map<string, value> dataReadBack(struct1);

        TEST(static_cast<int>(value_int(dataReadBack["the_integer"])) == 9);

        value val1(struct1);
        TEST(val1.type() == value::TYPE_STRUCT);
        value_struct struct2(val1);
        try {
            value_struct struct4(value_int(4));
            TEST_FAILED("invalid cast int-struct suceeded");
        } catch (error) {}
    }
};



class arrayTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "arrayTestSuite";
    }
    virtual void runtests(unsigned int const) {
        vector<value> arrayData;
        arrayData.push_back(value_int(7));
        arrayData.push_back(value_double(2.78));
        arrayData.push_back(value_string("hello world"));
        value_array array1(arrayData);

        TEST(array1.size() == 3);
        vector<value> dataReadBack1(array1.vectorValueValue());
        TEST(dataReadBack1[0].type() ==  value::TYPE_INT);
        TEST(static_cast<int>(value_int(dataReadBack1[0])) == 7);
        TEST(dataReadBack1[1].type() ==  value::TYPE_DOUBLE);
        TEST(static_cast<double>(value_double(dataReadBack1[1])) == 2.78);
        TEST(dataReadBack1[2].type() ==  value::TYPE_STRING);
        TEST(static_cast<string>(value_string(dataReadBack1[2])) == 
             "hello world");

        value val1(array1);
        TEST(val1.type() == value::TYPE_ARRAY);
        value_array array2(val1);
        TEST(array2.size() == 3);
        try {
            value_array array4(value_int(4));
            TEST_FAILED("invalid cast int-array suceeded");
        } catch (error) {}
    }
};



class valueTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "valueTestSuite";
    }
    virtual void runtests(unsigned int const indentation) {

        intTestSuite().run(indentation+1);
        doubleTestSuite().run(indentation+1);
        booleanTestSuite().run(indentation+1);
        datetimeTestSuite().run(indentation+1);
        stringTestSuite().run(indentation+1);
        bytestringTestSuite().run(indentation+1);
        nilTestSuite().run(indentation+1);
        i8TestSuite().run(indentation+1);
        structTestSuite().run(indentation+1);
        arrayTestSuite().run(indentation+1);
    }
};


class paramListTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "paramListTestSuite";
    }
    virtual void runtests(unsigned int const) {

        paramList paramList1;
        TEST(paramList1.size() == 0);

        paramList1.add(value_int(7));
        paramList1.add(value_boolean(true));
        paramList1.add(value_double(3.14));
        time_t const timeZero(0);
        paramList1.add(value_datetime(timeZero));
        time_t const timeFuture(time(NULL)+100);
        paramList1.add(value_datetime(timeFuture));
        paramList1.add(value_string("hello world"));
        unsigned char bytestringArray[] = {0x10, 0x11, 0x12, 0x13, 0x14};
        vector<unsigned char> 
            bytestringData(&bytestringArray[0], &bytestringArray[4]);
        paramList1.add(value_bytestring(bytestringData));
        vector<value> arrayData;
        arrayData.push_back(value_int(7));
        arrayData.push_back(value_double(2.78));
        arrayData.push_back(value_string("hello world"));
        paramList1.add(value_array(arrayData));
        map<string, value> structData;
        pair<string, value> member("the_integer", value_int(9));
        structData.insert(member);
        paramList1.add(value_struct(structData));
        paramList1.add(value_nil());
        paramList1.add(value_i8((xmlrpc_int64)UINT_MAX + 1));

        TEST(paramList1.size() == 11);

        TEST(paramList1.getInt(0) == 7);
        TEST(paramList1.getInt(0, 7) == 7);
        TEST(paramList1.getInt(0, -5, 7) == 7);
        TEST(paramList1.getBoolean(1) == true);
        TEST(paramList1.getDouble(2) == 3.14);
        TEST(paramList1.getDouble(2, 1) == 3.14);
        TEST(paramList1.getDouble(2, 1, 4) == 3.14);
        TEST(paramList1.getDatetime_sec(3) == 0);
        TEST(paramList1.getDatetime_sec(3, paramList::TC_ANY) == timeZero);
        TEST(paramList1.getDatetime_sec(3, paramList::TC_NO_FUTURE) 
             == timeZero);
        TEST(paramList1.getDatetime_sec(4, paramList::TC_NO_PAST)
             == timeFuture);
        TEST(paramList1.getString(5) == "hello world");
        TEST(paramList1.getBytestring(6)[0] == 0x10);
        TEST(paramList1.getArray(7).size() == 3);
        TEST(paramList1.getArray(7, 3).size() == 3);
        TEST(paramList1.getArray(7, 1, 3).size() == 3);
        paramList1.getStruct(8)["the_integer"];
        paramList1.getNil(9);
        TEST(paramList1.getI8(10) == (xmlrpc_int64)UINT_MAX + 1);
        paramList1.verifyEnd(11);

        paramList paramList2(5);
        TEST(paramList2.size() == 0);
    }
};

//=========================================================================
//  Test Driver
//=========================================================================

int 
main(int argc, char**) {
    
    int retval;

    if (argc-1 > 0) {
        cout << "Program takes no arguments" << endl;
        exit(1);
    }

    bool testsPassed;

    try {
        // Add your test suites here.
        valueTestSuite().run(0);
        paramListTestSuite().run(0);
        registryTestSuite().run(0);
        serverAbyssTestSuite().run(0);
#ifndef  WIN32
        serverPstreamTestSuite().run(0);
#endif
        clientTestSuite().run(0);

        testXmlRpcCpp();

        testsPassed = true;
    } catch (error const& error) {
        cout << "Unexpected error thrown:  " << error.what() << endl;
        testsPassed = false;
    } catch (XmlRpcFault const& fault) {
        cout << "Unexpected XML-RPC fault when running test suites." << endl
             << "Fault #" << fault.getFaultCode()
             << ": " << fault.getFaultString() << endl;
        testsPassed = false;
    } catch (...) {
        cout << "Unexpected exception when running test suites." << endl;
        testsPassed = false;
    }

    if (testsPassed) {
        cout << "PASSED" << endl;
        retval = 0;
    } else {
        cout << "FAILED" << endl;
        retval = 1;
    }
    return retval;
}
