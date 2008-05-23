#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/base.hpp"
#include "env_wrap.hpp"

#include "xmlrpc-c/xml.hpp"

using namespace std;
using namespace xmlrpc_c;


namespace {

class cValueWrapper {
/*----------------------------------------------------------------------------
   Use an object of this class to set up to remove a reference to an
   xmlrpc_value object (a C object with manual reference management)
   at then end of a scope -- even if the scope ends with a throw.
-----------------------------------------------------------------------------*/
public:
    xmlrpc_value * valueP;
    cValueWrapper(xmlrpc_value * valueP) : valueP(valueP) {}
    ~cValueWrapper() { xmlrpc_DECREF(valueP); }
};

xmlrpc_value *
cArrayFromParamList(paramList const& paramList) {

    env_wrap env;

    xmlrpc_value * paramArrayP;

    paramArrayP = xmlrpc_array_new(&env.env_c);
    if (!env.env_c.fault_occurred) {
        for (unsigned int i = 0;
             i < paramList.size() && !env.env_c.fault_occurred;
             ++i) {
            cValueWrapper const param(paramList[i].cValue());

            xmlrpc_array_append_item(&env.env_c, paramArrayP, param.valueP);
        }
    }
    if (env.env_c.fault_occurred) {
        xmlrpc_DECREF(paramArrayP);
        throw(error(env.env_c.fault_string));
    }
    return paramArrayP;
}

} // namespace


namespace xmlrpc_c {
namespace xml {


void
generateCall(string         const& methodName,
             paramList      const& paramList,
             xmlrpc_dialect const  dialect,
             string *       const  callXmlP) {
/*----------------------------------------------------------------------------
   Generate the XML for an XML-RPC call, given a method name and parameter
   list.

   Use dialect 'dialect' of XML-RPC.
-----------------------------------------------------------------------------*/
    class memblockWrapper {
        xmlrpc_mem_block * const memblockP;
    public:
        memblockWrapper(xmlrpc_mem_block * const memblockP) :
            memblockP(memblockP) {}

        ~memblockWrapper() {
            XMLRPC_MEMBLOCK_FREE(char, memblockP);
        }
    };

    xmlrpc_mem_block * callXmlMP;
    env_wrap env;

    callXmlMP = XMLRPC_MEMBLOCK_NEW(char, &env.env_c, 0);
    if (!env.env_c.fault_occurred) {
        memblockWrapper callXmlHolder(callXmlMP);
            // Makes callXmlMP get freed at end of scope

        xmlrpc_value * const paramArrayP(cArrayFromParamList(paramList));

        xmlrpc_serialize_call2(&env.env_c, callXmlMP, methodName.c_str(),
                               paramArrayP, dialect);
        
        *callXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, callXmlMP),
                           XMLRPC_MEMBLOCK_SIZE(char, callXmlMP));
        
        xmlrpc_DECREF(paramArrayP);
    }
    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
generateCall(string    const& methodName,
             paramList const& paramList,
             string *  const  callXmlP) {

    generateCall(methodName, paramList, xmlrpc_dialect_i8, callXmlP);

}



void
parseResponse(string       const& responseXml,
              rpcOutcome * const  outcomeP) {
/*----------------------------------------------------------------------------
   Parse the XML for an XML-RPC response into an XML-RPC result value.
-----------------------------------------------------------------------------*/
    env_wrap env;

    xmlrpc_value * c_resultP;
    int faultCode;
    const char * faultString;

    xmlrpc_parse_response2(&env.env_c, responseXml.c_str(), responseXml.size(),
                           &c_resultP, &faultCode, &faultString);

    if (env.env_c.fault_occurred)
        throwf("Unable to find XML-RPC response in what server sent back.  %s",
               env.env_c.fault_string);
    else {
        if (faultString) {
            *outcomeP =
                rpcOutcome(fault(faultString,
                                 static_cast<fault::code_t>(faultCode)));
            xmlrpc_strfree(faultString);
        } else {
            XMLRPC_ASSERT_VALUE_OK(c_resultP);
            *outcomeP = rpcOutcome(value(c_resultP));
            xmlrpc_DECREF(c_resultP);
        }
    }
}



void
parseSuccessfulResponse(string  const& responseXml,
                        value * const  resultP) {
/*----------------------------------------------------------------------------
   Same as parseResponse(), but expects the response to indicate success;
   throws an error if it doesn't.
-----------------------------------------------------------------------------*/
    rpcOutcome outcome;

    parseResponse(responseXml, &outcome);

    if (!outcome.succeeded())
        throwf("RPC response indicates it failed.  %s",
               outcome.getFault().getDescription().c_str());

    *resultP = outcome.getResult();
}



void
trace(string const& label,
      string const& xml) {
    
    xmlrpc_traceXml(label.c_str(), xml.c_str(), xml.size());

}


}} // namespace
