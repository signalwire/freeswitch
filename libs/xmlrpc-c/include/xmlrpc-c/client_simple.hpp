#ifndef CLIENT_SIMPLE_HPP_INCLUDED
#define CLIENT_SIMPLE_HPP_INCLUDED

#include <string>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client.hpp>

namespace xmlrpc_c {


class clientSimple {

public:
    clientSimple();

    void
    call(std::string       const serverUrl,
         std::string       const methodName,
         xmlrpc_c::value * const resultP);

    void
    call(std::string       const serverUrl,
         std::string       const methodName,
         std::string       const format,
         xmlrpc_c::value * const resultP,
         ...);

    void
    call(std::string         const  serverUrl,
         std::string         const  methodName,
         xmlrpc_c::paramList const& paramList,
         xmlrpc_c::value *   const  resultP);

private:
    xmlrpc_c::clientPtr clientP;
};

} // namespace
#endif




