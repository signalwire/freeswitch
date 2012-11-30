/*=============================================================================
                                  registry
===============================================================================
  Test the method registry (server) C++ facilities of XML-RPC for C/C++.
  
=============================================================================*/

#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/registry.hpp"

#include "tools.hpp"
#include "registry.hpp"

using namespace xmlrpc_c;
using namespace std;



namespace {

static string const
xmlPrologue("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");

static string const
apacheUrl("http://ws.apache.org/xmlrpc/namespaces/extensions");

static string const
xmlnsApache("xmlns:ex=\"" + apacheUrl + "\"");


string const noElementFoundXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<fault>\r\n"
    "<value><struct>\r\n"
    "<member><name>faultCode</name>\r\n"
    "<value><i4>-503</i4></value></member>\r\n"
    "<member><name>faultString</name>\r\n"
    "<value><string>Call XML not a proper XML-RPC call.  "
    "Call is not valid XML.  no element found</string></value>"
    "</member>\r\n"
    "</struct></value>\r\n"
    "</fault>\r\n"
    "</methodResponse>\r\n"
    );

string const invalidXMLCall(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<fault>\r\n"
    "<value><struct>\r\n"
    "<member><name>faultCode</name>\r\n"
    "<value><i4>-503</i4></value></member>\r\n"
    "<member><name>faultString</name>\r\n"
    "<value><string>Call XML not a proper XML-RPC call.  "
    "Call is not valid XML.  XML parsing failed</string></value>"
    "</member>\r\n"
    "</struct></value>\r\n"
    "</fault>\r\n"
    "</methodResponse>\r\n"
    );

string const sampleAddGoodCallXml(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>sample.add</methodName>\r\n"
    "<params>\r\n"
    "<param><value><i4>5</i4></value></param>\r\n"
    "<param><value><i4>7</i4></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const sampleAddGoodResponseXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<params>\r\n"
    "<param><value><i4>12</i4></value></param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );


string const sampleAddBadCallXml(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>sample.add</methodName>\r\n"
    "<params>\r\n"
    "<param><value><i4>5</i4></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const sampleAddBadResponseXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<fault>\r\n"
    "<value><struct>\r\n"
    "<member><name>faultCode</name>\r\n"
    "<value><i4>-501</i4></value></member>\r\n"
    "<member><name>faultString</name>\r\n"
    "<value><string>Not enough parameters</string></value></member>\r\n"
    "</struct></value>\r\n"
    "</fault>\r\n"
    "</methodResponse>\r\n"
    );

string const testCallInfoCallXml(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>test.callinfo</methodName>\r\n"
    "<params>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const testCallInfoResponseXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<params>\r\n"
    "<param><value><string>this is a test callInfo</string></value>"
    "</param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );

string const nonexistentMethodCallXml(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>nosuchmethod</methodName>\r\n"
    "<params>\r\n"
    "<param><value><i4>5</i4></value></param>\r\n"
    "<param><value><i4>7</i4></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const nonexistentMethodYesDefResponseXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<params>\r\n"
    "<param><value><string>no such method: nosuchmethod</string>"
    "</value></param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );

string const nonexistentMethodNoDefResponseXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<fault>\r\n"
    "<value><struct>\r\n"
    "<member><name>faultCode</name>\r\n"
    "<value><i4>-506</i4></value></member>\r\n"
    "<member><name>faultString</name>\r\n"
    "<value><string>Method 'nosuchmethod' not defined</string></value>"
    "</member>\r\n"
    "</struct></value>\r\n"
    "</fault>\r\n"
    "</methodResponse>\r\n"
    );



string const echoI8ApacheCall(
    xmlPrologue +
    "<methodCall " + xmlnsApache + ">\r\n"
    "<methodName>echo</methodName>\r\n"
    "<params>\r\n"
    "<param><value><ex:i8>5</ex:i8></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const echoI8ApacheResponse(
    xmlPrologue +
    "<methodResponse " + xmlnsApache + ">\r\n"
    "<params>\r\n"
    "<param><value><ex:i8>5</ex:i8></value></param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );

string const echoNilApacheCall(
    xmlPrologue +
    "<methodCall " + xmlnsApache + ">\r\n"
    "<methodName>echo</methodName>\r\n"
    "<params>\r\n"
    "<param><value><nil/></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const echoNilApacheResponse(
    xmlPrologue +
    "<methodResponse " + xmlnsApache + ">\r\n"
    "<params>\r\n"
    "<param><value><ex:nil/></value></param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );


class callInfo_test : public callInfo {

public:
    callInfo_test() : data("this is a test callInfo") {}

    callInfo_test(string const& data) : data(data) {};

    string data;
};



class sampleAddMethod : public method {
public:
    sampleAddMethod() {
        this->_signature = "i:ii";
        this->_help = "This method adds two integers together";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            value *             const  retvalP) {
        
        int const addend(paramList.getInt(0));
        int const adder(paramList.getInt(1));
        
        paramList.verifyEnd(2);
        
        *retvalP = value_int(addend + adder);
    }
};



class sampleAddMethod2 : public method2 {
public:
    sampleAddMethod2() {
        this->_signature = "i:ii";
        this->_help = "This method adds two integers together";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            const callInfo *    const,
            value *             const  retvalP) {
        
        int const addend(paramList.getInt(0));
        int const adder(paramList.getInt(1));
        
        paramList.verifyEnd(2);
        
        *retvalP = value_int(addend + adder);
    }
};



class testCallInfoMethod : public method2 {
public:
    testCallInfoMethod() {
        this->_signature = "s:";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            const callInfo *    const  callInfoPtr,
            value *             const  retvalP) {
        
        const callInfo_test * const callInfoP(
            dynamic_cast<const callInfo_test *>(callInfoPtr));

        TEST(callInfoP != NULL);

        paramList.verifyEnd(0);
        
        *retvalP = value_string(callInfoP->data);
    }
};



class nameMethod : public defaultMethod {

    void
    execute(string              const& methodName,
            xmlrpc_c::paramList const& ,  // paramList
            value *             const  retvalP) {
        
        *retvalP = value_string(string("no such method: ") + methodName);
    }
};



class echoMethod : public method {
public:
    void
    execute(xmlrpc_c::paramList const& paramList,
            value *             const  retvalP) {
        
        paramList.verifyEnd(1);
        
        *retvalP = paramList[0];
    }
};



static void
testEmptyXmlDocCall(xmlrpc_c::registry const& myRegistry) {

    string response;
    myRegistry.processCall("", &response);

#ifdef INTERNAL_EXPAT
    TEST(response == noElementFoundXml);
#else
    // This is what we get with libxml2
    TEST(response == invalidXMLCall);
#endif
}



class registryRegMethodTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "registryRegMethodTestSuite";
    }
    virtual void runtests(unsigned int const) {

        xmlrpc_c::registry myRegistry;
        
        myRegistry.addMethod("sample.add", 
                             xmlrpc_c::methodPtr(new sampleAddMethod));
        
        myRegistry.disableIntrospection();
        testEmptyXmlDocCall(myRegistry);
        {
            string response;
            myRegistry.processCall(sampleAddGoodCallXml, &response);
            TEST(response == sampleAddGoodResponseXml);
        }
        {
            string response;
            myRegistry.processCall(sampleAddBadCallXml, &response);
            TEST(response == sampleAddBadResponseXml);
        }
        {
            string response;
            callInfo const callInfo;
            myRegistry.processCall(sampleAddBadCallXml, &callInfo, &response);
            TEST(response == sampleAddBadResponseXml);
        }
    }
};



class registryDefaultMethodTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "registryDefaultMethodTestSuite";
    }
    virtual void runtests(unsigned int const) {

        xmlrpc_c::registry myRegistry;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));

        {
            string response;
            myRegistry.processCall(sampleAddGoodCallXml, &response);
            TEST(response == sampleAddGoodResponseXml);
        }
        {
            string response;
            myRegistry.processCall(nonexistentMethodCallXml, &response);
            TEST(response == nonexistentMethodNoDefResponseXml);
        }
        // We're actually violating the spirit of setDefaultMethod by
        // doing this to a registry that's already been used, but as long
        // as it works, it's a convenient way to implement this test.
        myRegistry.setDefaultMethod(defaultMethodPtr(new nameMethod));

        {
            string response;
            myRegistry.processCall(nonexistentMethodCallXml, &response);
            TEST(response == nonexistentMethodYesDefResponseXml);
        }
    }
};



class method2TestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "method2TestSuite";
    }
    virtual void runtests(unsigned int const) {

        xmlrpc_c::registry myRegistry;
        
        myRegistry.addMethod("sample.add", 
                             xmlrpc_c::methodPtr(new sampleAddMethod2));
        
        myRegistry.addMethod("test.callinfo", 
                             xmlrpc_c::methodPtr(new testCallInfoMethod));
        
        {
            string response;
            myRegistry.processCall(sampleAddGoodCallXml, &response);
            TEST(response == sampleAddGoodResponseXml);
        }
        {
            string response;
            myRegistry.processCall(sampleAddBadCallXml, &response);
            TEST(response == sampleAddBadResponseXml);
        }
        {
            string response;
            callInfo_test const callInfo;
            myRegistry.processCall(testCallInfoCallXml, &callInfo, &response);
            TEST(response == testCallInfoResponseXml);
        }
    }
};



class dialectTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "dialectTestSuite";
    }
    virtual void runtests(unsigned int const) {

        registry myRegistry;
        string response;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));
        myRegistry.addMethod("echo", methodPtr(new echoMethod));

        myRegistry.setDialect(xmlrpc_dialect_i8);

        myRegistry.setDialect(xmlrpc_dialect_apache);

        myRegistry.processCall(echoI8ApacheCall, &response);

        TEST(response == echoI8ApacheResponse);

        myRegistry.processCall(echoNilApacheCall, &response);

        TEST(response == echoNilApacheResponse);

        EXPECT_ERROR(  // invalid dialect
            myRegistry.setDialect(static_cast<xmlrpc_dialect>(300));
            );
    }
};



class testShutdown : public xmlrpc_c::registry::shutdown {
/*----------------------------------------------------------------------------
   This class is logically local to
   registryShutdownTestSuite::runtests(), but if we declare it that
   way, gcc 2.95.3 fails with some bogus messages about undefined
   references from random functions when we do that.
-----------------------------------------------------------------------------*/
public:
    void doit(string const&,
              void * const) const {
        
    }
};



class registryShutdownTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "registryShutdownTestSuite";
    }
    virtual void runtests(unsigned int const) {

        xmlrpc_c::registry myRegistry;

        testShutdown shutdown;
        
        myRegistry.setShutdown(&shutdown);
    }
};



} // unnamed namespace



string
registryTestSuite::suiteName() {
    return "registryTestSuite";
}



void
registryTestSuite::runtests(unsigned int const indentation) {

    {
        registryPtr myRegistryP(new registry);
    
        myRegistryP->addMethod("sample.add", methodPtr(new sampleAddMethod));
    }

    registryRegMethodTestSuite().run(indentation+1);

    registryDefaultMethodTestSuite().run(indentation+1);

    method2TestSuite().run(indentation+1);

    registry myRegistry;

    myRegistry.disableIntrospection();

    dialectTestSuite().run(indentation+1);

    registryShutdownTestSuite().run(indentation+1);

    TEST(myRegistry.maxStackSize() >= 256);

}
