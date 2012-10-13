#ifndef SERVER_CGI_HPP_INCLUDED
#define SERVER_CGI_HPP_INCLUDED

#include <xmlrpc-c/c_util.h>
#include <xmlrpc-c/registry.hpp>

namespace xmlrpc_c {

class XMLRPC_DLLEXPORT serverCgi {

public:

    class XMLRPC_DLLEXPORT constrOpt {
    public:
        constrOpt();

        constrOpt & registryPtr       (xmlrpc_c::registryPtr      const& arg);
        constrOpt & registryP         (const xmlrpc_c::registry * const& arg);

        struct value {
            xmlrpc_c::registryPtr      registryPtr;
            const xmlrpc_c::registry * registryP;
        } value;
        struct {
            bool registryPtr;
            bool registryP;
        } present;
    };

    serverCgi(constrOpt const& opt);

    ~serverCgi();

    void
    processCall();

private:

    struct serverCgi_impl * implP;
};


} // namespace

#endif
