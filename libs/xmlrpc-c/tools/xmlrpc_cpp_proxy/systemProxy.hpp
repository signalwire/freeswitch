#ifndef SYSTEMPROXY_HPP_INCLUDED
#define SYSTEMPROXY_HPP_INCLUDED

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

class systemProxy : public xmlrpc_c::clientSimple {

public:
    systemProxy() {}

    xmlrpc_c::value /*array*/
    listMethods(std::string const& serverUrl);

    xmlrpc_c::value /*array*/
    methodSignature(std::string const& serverUrl,
                    std::string const& methodName);

    std::string
    methodHelp(std::string const& serverUrl,
               std::string const& methodName);
};

#endif

