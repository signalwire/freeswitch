#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <memory>
#include <cstring>
#include <time.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "transport_config.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/oldcppwrapper.hpp"
#include "xmlrpc-c/registry.hpp"
#include "c_util.h"

#include "tools.hpp"

#include "value.hpp"

using namespace xmlrpc_c;
using namespace std;



namespace {

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

        value const int1x(toValue(7));
        TEST(int1x.type() == value::TYPE_INT);
        TEST(static_cast<int>(value_int(int1x)) == 7);

        int test1x;
        fromValue(test1x, int1x);
        TEST(test1x == 7);
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

        value const double1x(toValue(3.14));
        TEST(double1x.type() == value::TYPE_DOUBLE);
        TEST(static_cast<double>(value_double(double1x)) == 3.14);

        double test1x;
        fromValue(test1x, double1x);
        TEST(test1x == 3.14);
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

        value const boolean1x(toValue(true));
        TEST(boolean1x.type() == value::TYPE_BOOLEAN);
        TEST(static_cast<bool>(value_boolean(boolean1x)) == true);

        bool test1x;
        fromValue(test1x, boolean1x);
        TEST(test1x == true);
    }
};



#if XMLRPC_HAVE_TIMEVAL

static struct timeval
makeTv(time_t       const secs,
       unsigned int const usecs) {

    struct timeval retval;

    retval.tv_sec  = secs;
    retval.tv_usec = usecs;

    return retval;
}

static bool
tvIsEqual(struct timeval const comparand,
          struct timeval const comparator) {
    return
        comparand.tv_sec  == comparator.tv_sec &&
        comparand.tv_usec == comparator.tv_usec;
}
#endif



#if XMLRPC_HAVE_TIMESPEC

static struct timespec
makeTs(time_t       const secs,
       unsigned int const usecs) {

    struct timespec retval;

    retval.tv_sec  = secs;
    retval.tv_nsec = usecs * 1000;

    return retval;
}

static bool
tsIsEqual(struct timespec const comparand,
          struct timespec const comparator) {
    return
        comparand.tv_sec  == comparator.tv_sec &&
        comparand.tv_nsec == comparator.tv_nsec;
}
#endif



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
#if XMLRPC_HAVE_TIMEVAL
        struct timeval const testTimeTv(makeTv(testTime, 0));
        value_datetime datetime4(testTimeTv);
        TEST(static_cast<time_t>(datetime4) == testTime);
        TEST(tvIsEqual(static_cast<timeval>(datetime4), testTimeTv));
#endif
#if XMLRPC_HAVE_TIMESPEC
        struct timespec const testTimeTs(makeTs(testTime, 0));
        value_datetime datetime5(testTimeTs);
        TEST(static_cast<time_t>(datetime5) == testTime);
        TEST(tsIsEqual(static_cast<timespec>(datetime5), testTimeTs));
#endif
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

        value const string1x(toValue("hello world"));
        TEST(string1x.type() == value::TYPE_STRING);
        TEST(static_cast<string>(value_string(string1x)) == "hello world");

        string test1x;
        fromValue(test1x, string1x);
        TEST(test1x == "hello world");

        value const string2x(toValue(string("hello world")));
        TEST(string2x.type() == value::TYPE_STRING);
        TEST(static_cast<string>(value_string(string2x)) == "hello world");
    }
};



class bytestringTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "bytestringTestSuite";
    }
    virtual void runtests(unsigned int const) {
        unsigned char bytestringArray[] = {0x10, 0x11, 0x12, 0x13, 0x14};
        cbytestring
            bytestringData(&bytestringArray[0], &bytestringArray[4]);
        value_bytestring bytestring1(bytestringData);

        cbytestring const dataReadBack1(bytestring1.vectorUcharValue());
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

        value const bytestring1x(toValue(bytestringData));
        TEST(bytestring1x.type() == value::TYPE_BYTESTRING);
        vector<unsigned char> const dataReadBack1x(
            value_bytestring(bytestring1x).vectorUcharValue());
        TEST(dataReadBack1x == bytestringData);

        vector<unsigned char> test1x;
        fromValue(test1x, bytestring1x);
        TEST(test1x == bytestringData);

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
        cstruct structData;
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

        map<string, int> structDatax;
        structDatax["one"] = 1;
        structDatax["two"] = 2;

        value const struct5(toValue(structDatax));
        TEST(struct5.type() == value::TYPE_STRUCT);
        map<string, value> dataReadBackx;
        dataReadBackx = value_struct(struct5);

        TEST(static_cast<int>(value_int(dataReadBackx["two"])) == 2);

        map<string, int> test5x;
        fromValue(test5x, struct5);
        TEST(test5x["two"] == 2);
    }
};



class arrayTestSuite : public testSuite {
public:
    virtual string suiteName() {
        return "arrayTestSuite";
    }
    virtual void runtests(unsigned int const) {
        carray arrayData;
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

        int const arrayDatax[] = {7, 4};

        value const array5(
            arrayValueArray(arrayDatax, ARRAY_SIZE(arrayDatax)));
        TEST(array5.type() == value::TYPE_ARRAY);
        TEST(value_array(array5).size() == 2);
        vector<value> dataReadBackx(value_array(array5).vectorValueValue());

        TEST(dataReadBackx.size() == 2);
        TEST(static_cast<int>(value_int(dataReadBackx[0])) == 7);
        vector<int> test5x;
        fromValue(test5x, array5);
        TEST(test5x[1] == 4);

        vector<string> arrayDataVec;
        arrayDataVec.push_back("hello world");
        value const array6(toValue(arrayDataVec));
        TEST(array6.type() == value::TYPE_ARRAY);
        TEST(value_array(array6).size() == 1);
    }
};


} // unnamed namespace


string
valueTestSuite::suiteName() {
    return "valueTestSuite";
}



void
valueTestSuite::runtests(unsigned int const indentation) {

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
