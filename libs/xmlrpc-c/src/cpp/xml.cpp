#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base_int.h"
#include "xmlrpc-c/base.hpp"
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

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_value * paramArrayP;

    paramArrayP = xmlrpc_array_new(&env);
    if (!env.fault_occurred) {
        for (unsigned int i = 0;
             i < paramList.size() && !env.fault_occurred;
             ++i) {
            cValueWrapper const param(paramList[i].cValue());

            xmlrpc_array_append_item(&env, paramArrayP, param.valueP);
        }
    }
    if (env.fault_occurred) {
        xmlrpc_DECREF(paramArrayP);
        throw(error(env.fault_string));
    }
    return paramArrayP;
}

} // namespace


namespace xmlrpc_c {
namespace xml {


void
generateCall(string    const& methodName,
             paramList const& paramList,
             string *  const  callXmlP) {
/*----------------------------------------------------------------------------
   Generate the XML for an XML-RPC call, given a method name and parameter
   list.
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
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    callXmlMP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);
    if (!env.fault_occurred) {
        memblockWrapper callXmlHolder(callXmlMP);
            // Makes callXmlMP get freed at end of scope

        xmlrpc_value * const paramArrayP(cArrayFromParamList(paramList));

        xmlrpc_serialize_call(&env, callXmlMP, methodName.c_str(),
                              paramArrayP);
        
        *callXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, callXmlMP),
                           XMLRPC_MEMBLOCK_SIZE(char, callXmlMP));
        
        xmlrpc_DECREF(paramArrayP);
    }
    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
parseResponse(string       const& responseXml,
              rpcOutcome * const  outcomeP) {
/*----------------------------------------------------------------------------
   Parse the XML for an XML-RPC response into an XML-RPC result value.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    xmlrpc_value * c_resultP;

    c_resultP = 
        xmlrpc_parse_response(&env, responseXml.c_str(), responseXml.size());

    /* Unfortunately, xmlrpc_parse_response() does not distinguish between
       unparseable XML and XML that cleanly indicates an RPC failure or
       other failure on the server end.  We'll fix that some day, but for
       now, we just assume any failure is an XML-RPC RPC failure.
    */
    if (env.fault_occurred)
        *outcomeP =
            rpcOutcome(fault(env.fault_string,
                             static_cast<fault::code_t>(env.fault_code)));
    else {
        *outcomeP = rpcOutcome(value(c_resultP));
        xmlrpc_DECREF(c_resultP);
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
        throw(error("RPC failed.  " + outcome.getFault().getDescription()));

    *resultP = outcome.getResult();
}



void
trace(string const& label,
      string const& xml) {
    
    xmlrpc_traceXml(label.c_str(), xml.c_str(), xml.size());

}


}} // namespace
