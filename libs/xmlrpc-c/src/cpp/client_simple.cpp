#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/client.hpp"
#include <xmlrpc-c/client.hpp>
/* transport_config.h defines XMLRPC_DEFAULT_TRANSPORT,
    MUST_BUILD_WININET_CLIENT, MUST_BUILD_CURL_CLIENT,
    MUST_BUILD_LIBWWW_CLIENT 
*/
#include "transport_config.h"

#include "xmlrpc-c/client_simple.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {


namespace {
/*----------------------------------------------------------------------------
   Use an object of this class to set up to remove a reference to an
   xmlrpc_value object (a C object with manual reference management)
   at then end of a scope -- even if the scope ends with a throw.
-----------------------------------------------------------------------------*/
class cValueWrapper {
    xmlrpc_value * valueP;
public:
    cValueWrapper(xmlrpc_value * valueP) : valueP(valueP) {}
    ~cValueWrapper() { xmlrpc_DECREF(valueP); }
};

} // namespace



clientSimple::clientSimple() {
    
    if (string(XMLRPC_DEFAULT_TRANSPORT) == string("curl"))
        this->transportP = new clientXmlTransport_curl;
    else if (string(XMLRPC_DEFAULT_TRANSPORT) == string("libwww"))
        this->transportP = new clientXmlTransport_libwww;
    else if (string(XMLRPC_DEFAULT_TRANSPORT) == string("wininet"))
        this->transportP = new clientXmlTransport_wininet;
    else
        throw(error("INTERNAL ERROR: "
                    "Default client XML transport is not one we recognize"));

    this->clientP = new client_xml(transportP);
}



clientSimple::~clientSimple() {

    delete this->clientP;
    delete this->transportP;
}



void
clientSimple::call(string  const serverUrl,
                   string  const methodName,
                   value * const resultP) {

    carriageParm_http0 carriageParm(serverUrl);

    rpcPtr rpcPtr(methodName, paramList());

    rpcPtr->call(this->clientP, &carriageParm);
    
    *resultP = rpcPtr->getResult();
}


namespace {

void
makeParamArray(string          const format,
               xmlrpc_value ** const paramArrayPP,
               va_list               args) {
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    /* The format is a sequence of parameter specifications, such as
       "iiii" for 4 integer parameters.  We add parentheses to make it
       an array of those parameters: "(iiii)".
    */
    string const arrayFormat("(" + string(format) + ")");
    const char * tail;

    xmlrpc_build_value_va(&env, arrayFormat.c_str(),
                          args, paramArrayPP, &tail);

    if (env.fault_occurred)
        throw(error(env.fault_string));

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

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_value * paramArrayP;

    va_list args;
    va_start(args, resultP);
    makeParamArray(format, &paramArrayP, args);
    va_end(args);

    if (env.fault_occurred)
        throw(error(env.fault_string));
    else {
        cValueWrapper paramArrayWrapper(paramArrayP); // ensure destruction
        unsigned int const paramCount = xmlrpc_array_size(&env, paramArrayP);
        
        if (env.fault_occurred)
            throw(error(env.fault_string));
        
        paramList paramList;
        for (unsigned int i = 0; i < paramCount; ++i) {
            xmlrpc_value * paramP;
            xmlrpc_array_read_item(&env, paramArrayP, i, &paramP);
            if (env.fault_occurred)
                throw(error(env.fault_string));
            else {
                cValueWrapper paramWrapper(paramP); // ensure destruction
                paramList.add(value(paramP));
            }
        }
        rpcPtr rpcPtr(methodName, paramList);
        rpcPtr->call(this->clientP, &carriageParm);
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

    rpcPtr->call(this->clientP, &carriageParm);
    
    *resultP = rpcPtr->getResult();
}

} // namespace
