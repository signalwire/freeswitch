/*=============================================================================
                                   xml
===============================================================================
  Test the XML generator and parser C++ facilities of XML-RPC for C/C++.
  
=============================================================================*/

#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/xml.hpp"

#include "tools.hpp"
#include "xml.hpp"

using namespace xmlrpc_c;
using namespace std;


namespace  {

class callTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "callTestSuite";
    }
    virtual void runtests(unsigned int const) {

        string callXml;

        string const methodName0("myMethod");
        paramList const paramList0;

        xml::generateCall(methodName0, paramList(), &callXml);

        string methodName;
        paramList paramList;

        xml::parseCall(callXml, &methodName, &paramList);

        TEST(methodName == methodName0);
        TEST(paramList.size() == paramList0.size());
    }
};



class responseTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "responseTestSuite";
    }
    virtual void runtests(unsigned int const) {

        string respXml;

        rpcOutcome outcome0(value_int(7));

        xml::generateResponse(outcome0, &respXml);

        rpcOutcome outcome;

        xml::parseResponse(respXml, &outcome);
        
        TEST((int)value_int(outcome.getResult()) ==
             (int)value_int(outcome0.getResult()));

        value result;

        xml::parseSuccessfulResponse(respXml, &result);

        TEST((int)value_int(result) == (int)value_int(outcome0.getResult()));
    }
};



}  // unnamed namespace



string
xmlTestSuite::suiteName() {
    return "XMLTestSuite";
}



void
xmlTestSuite::runtests(unsigned int const indentation) {

    callTestSuite().run(indentation+1);

    responseTestSuite().run(indentation+1);
}
