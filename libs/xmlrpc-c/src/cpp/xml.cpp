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
   at the end of a scope -- even if the scope ends with a throw.
-----------------------------------------------------------------------------*/
public:
    xmlrpc_value * const valueP;
    cValueWrapper(xmlrpc_value * valueP) : valueP(valueP) {}
    ~cValueWrapper() { xmlrpc_DECREF(valueP); }
};



class cStringWrapper {
public:
    const char * const cString;
    cStringWrapper(const char * const cString) : cString(cString) {}
    ~cStringWrapper() { xmlrpc_strfree(cString); }
};
    


class memblockWrapper {
    xmlrpc_mem_block * const memblockP;
public:
    memblockWrapper(xmlrpc_mem_block * const memblockP) :
        memblockP(memblockP) {}

    ~memblockWrapper() {
        XMLRPC_MEMBLOCK_FREE(char, memblockP);
    }
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



paramList const 
paramListFromCArray(xmlrpc_value * const cArrayP) {

    paramList retval;
    env_wrap env;

    unsigned int const nParam(xmlrpc_array_size(&env.env_c, cArrayP));

    if (!env.env_c.fault_occurred) {
        for (unsigned int i = 0;
             i < nParam && !env.env_c.fault_occurred;
             ++i) {

            xmlrpc_value * cParamP;

            xmlrpc_array_read_item(&env.env_c, cArrayP, i, &cParamP);

            if (!env.env_c.fault_occurred) {

                cValueWrapper const paramAuto(cParamP);
                    // Causes xmlrpc_DECREF(cParamP) at end of scope

                retval.add(cParamP);
            }
        }
    }
    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));

    return retval;
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
parseCall(string      const& callXml,
          string *    const  methodNameP,
          paramList * const  paramListP) {

    env_wrap env;
    const char * c_methodName;
    xmlrpc_value * c_paramArrayP;

    xmlrpc_parse_call(&env.env_c, callXml.c_str(), callXml.size(),
                      &c_methodName, &c_paramArrayP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
    else {
        cValueWrapper const paramListAuto(c_paramArrayP);
            // Causes XMLRPC_decref(c_paramArrayP) at end of scope
        cStringWrapper const methodNameAuto(c_methodName);
            // Causes xmlrpc_strfree(c_methodName) at end of scope

        *paramListP  = paramListFromCArray(c_paramArrayP);
        *methodNameP = string(c_methodName);
    }
}



void
generateResponse(rpcOutcome     const& outcome,
                 xmlrpc_dialect const  dialect,
                 string *       const  respXmlP) {
/*----------------------------------------------------------------------------
   Generate the XML for an XML-RPC resp, given the RPC outcome.

   Use dialect 'dialect' of XML-RPC.
-----------------------------------------------------------------------------*/
    xmlrpc_mem_block * respXmlMP;
    env_wrap env;

    respXmlMP = XMLRPC_MEMBLOCK_NEW(char, &env.env_c, 0);
    if (!env.env_c.fault_occurred) {
        memblockWrapper respXmlAuto(respXmlMP);
            // Makes respXmlMP get freed at end of scope

        if (outcome.succeeded()) {
            cValueWrapper cResult(outcome.getResult().cValue());

            xmlrpc_serialize_response2(&env.env_c, respXmlMP,
                                       cResult.valueP, dialect);
        
            *respXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, respXmlMP),
                                   XMLRPC_MEMBLOCK_SIZE(char, respXmlMP));
        } else {
            env_wrap cFault;

            xmlrpc_env_set_fault(&cFault.env_c, outcome.getFault().getCode(),
                                 outcome.getFault().getDescription().c_str());

            xmlrpc_serialize_fault(&env.env_c, respXmlMP, &cFault.env_c);
        
            *respXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, respXmlMP),
                                   XMLRPC_MEMBLOCK_SIZE(char, respXmlMP));
        }
    }
    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
generateResponse(rpcOutcome const& outcome,
                 string *   const  respXmlP) {

    generateResponse(outcome, xmlrpc_dialect_i8, respXmlP);

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
