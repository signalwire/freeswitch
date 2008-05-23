#include <string>
#include <cstring>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "env_wrap.hpp"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/client.hpp"
#include <xmlrpc-c/client.hpp>

#include "xmlrpc-c/client_simple.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {


namespace {

void
throwIfError(env_wrap const& env) {

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}


class cValueWrapper {
/*----------------------------------------------------------------------------
   Use an object of this class to set up to remove a reference to an
   xmlrpc_value object (a C object with manual reference management)
   at then end of a scope -- even if the scope ends with a throw.
-----------------------------------------------------------------------------*/
    xmlrpc_value * valueP;
public:
    cValueWrapper(xmlrpc_value * valueP) : valueP(valueP) {}
    ~cValueWrapper() { xmlrpc_DECREF(valueP); }
};

} // namespace



clientSimple::clientSimple() {
    
    clientXmlTransportPtr const transportP(clientXmlTransport_http::create());

    this->clientP = clientPtr(new client_xml(transportP));
}



void
clientSimple::call(string  const serverUrl,
                   string  const methodName,
                   value * const resultP) {

    carriageParm_http0 carriageParm(serverUrl);

    rpcPtr rpcPtr(methodName, paramList());

    rpcPtr->call(this->clientP.get(), &carriageParm);
    
    *resultP = rpcPtr->getResult();
}


namespace {

void
makeParamArray(string          const format,
               xmlrpc_value ** const paramArrayPP,
               va_list               args) {
    
    env_wrap env;

    /* The format is a sequence of parameter specifications, such as
       "iiii" for 4 integer parameters.  We add parentheses to make it
       an array of those parameters: "(iiii)".
    */
    string const arrayFormat("(" + string(format) + ")");
    const char * tail;

    xmlrpc_build_value_va(&env.env_c, arrayFormat.c_str(),
                          args, paramArrayPP, &tail);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));

    if (strlen(tail) != 0) {
        /* xmlrpc_build_value_va() parses off a single value specification
           from its format string, and 'tail' points to whatever is after
           it.  Our format string should have been a single array value,
           meaning tail is end-of-string.  If it's not, that means
           something closed our array early.
        */
        xmlrpc_DECREF(*paramArrayPP);
        throw(error("format string is invalid.  It apparently has a "
                    "stray right parenthesis"));
    }
}

}  // namespace


void
clientSimple::call(string  const serverUrl,
                   string  const methodName,
                   string  const format,
                   value * const resultP,
                   ...) {

    carriageParm_http0 carriageParm(serverUrl);

    env_wrap env;
    xmlrpc_value * paramArrayP;

    va_list args;
    va_start(args, resultP);
    makeParamArray(format, &paramArrayP, args);
    va_end(args);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
    else {
        cValueWrapper paramArrayWrapper(paramArrayP); // ensure destruction
        unsigned int const paramCount(
            xmlrpc_array_size(&env.env_c, paramArrayP));
        
        if (env.env_c.fault_occurred)
            throw(error(env.env_c.fault_string));
        
        paramList paramList;
        for (unsigned int i = 0; i < paramCount; ++i) {
            xmlrpc_value * paramP;
            xmlrpc_array_read_item(&env.env_c, paramArrayP, i, &paramP);
            if (env.env_c.fault_occurred)
                throw(error(env.env_c.fault_string));
            else {
                cValueWrapper paramWrapper(paramP); // ensure destruction
                paramList.add(value(paramP));
            }
        }
        rpcPtr rpcPtr(methodName, paramList);
        rpcPtr->call(this->clientP.get(), &carriageParm);
        *resultP = rpcPtr->getResult();
    }
}



void
clientSimple::call(string    const  serverUrl,
                   string    const  methodName,
                   paramList const& paramList,
                   value *   const  resultP) {
    
    carriageParm_http0 carriageParm(serverUrl);
    
    rpcPtr rpcPtr(methodName, paramList);

    rpcPtr->call(this->clientP.get(), &carriageParm);
    
    *resultP = rpcPtr->getResult();
}

} // namespace
