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


string const xmlPrologue("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");



namespace {
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

} // namespace


string const echoI8ApacheCall(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>echo</methodName>\r\n"
    "<params>\r\n"
    "<param><value><ex.i8>5</ex.i8></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const echoI8ApacheResponse(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<params>\r\n"
    "<param><value><ex.i8>5</ex.i8></value></param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );

string const echoNilApacheCall(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>echo</methodName>\r\n"
    "<params>\r\n"
    "<param><value><nil/></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const echoNilApacheResponse(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<params>\r\n"
    "<param><value><ex.nil/></value></param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );


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
        {
            string response;
            myRegistry.processCall("", &response);
            TEST(response == noElementFoundXml);
        }
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



class registryShutdownTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "registryShutdownTestSuite";
    }
    virtual void runtests(unsigned int const) {

        xmlrpc_c::registry myRegistry;

        class myshutdown : public xmlrpc_c::registry::shutdown {
        public:
            void doit(string const&,
                      void * const) const {
                
            }
        };

        myshutdown shutdown;
        
        myRegistry.setShutdown(&shutdown);
    }
};



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

    registry myRegistry;
        
    myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));
    myRegistry.addMethod("echo", methodPtr(new echoMethod));

    string response;

    myRegistry.disableIntrospection();

    myRegistry.setDialect(xmlrpc_dialect_i8);

    myRegistry.setDialect(xmlrpc_dialect_apache);

    registryShutdownTestSuite().run(indentation+1);

    myRegistry.processCall(echoI8ApacheCall, &response);

    TEST(response == echoI8ApacheResponse);

    myRegistry.processCall(echoNilApacheCall, &response);

    TEST(response == echoNilApacheResponse);

    EXPECT_ERROR(  // invalid dialect
        myRegistry.setDialect(static_cast<xmlrpc_dialect>(300));
        );
}
