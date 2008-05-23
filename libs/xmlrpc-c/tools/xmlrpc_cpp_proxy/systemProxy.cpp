/*=============================================================================
                              systemProxy
===============================================================================

  This is a proxy class for the introspection system methods of the server.

  Note that you can use 'xmlrpc_cpp_proxy' itself to generate this
  file, but we hand-edit it to make it easier to read.

=============================================================================*/

#include "systemProxy.hpp"

using namespace std;

xmlrpc_c::value // array
systemProxy::listMethods(string const& serverUrl) {

    xmlrpc_c::paramList params;
    xmlrpc_c::value result;

    this->call(serverUrl, "system.listMethods", &result);

    return result;
}



xmlrpc_c::value // array
systemProxy::methodSignature(string const& serverUrl,
                             string const& methodName) {

    xmlrpc_c::paramList params;
    params.add(xmlrpc_c::value_string(methodName));

    xmlrpc_c::value result;

    this->call(serverUrl, "system.methodSignature", params, &result);

    return result;
}



string
systemProxy::methodHelp(string const& serverUrl,
                        string const& methodName) {

    xmlrpc_c::paramList params;
    params.add(xmlrpc_c::value_string(methodName));

    xmlrpc_c::value result;

    this->call(serverUrl, "system.methodHelp", params, &result);

    return xmlrpc_c::value_string(result);
}
