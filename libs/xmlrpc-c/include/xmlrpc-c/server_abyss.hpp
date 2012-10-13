#ifndef SERVER_ABYSS_HPP_INCLUDED
#define SERVER_ABYSS_HPP_INCLUDED

#ifdef WIN32
#include <winsock.h>   // For XMLRPC_SOCKET (= SOCKET)
#endif

#include <xmlrpc-c/config.h>  // For XMLRPC_SOCKET
#include <xmlrpc-c/c_util.h>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/abyss.h>

namespace xmlrpc_c {

struct serverAbyss_impl;

class XMLRPC_DLLEXPORT serverAbyss {
    
public:
    struct constrOpt_impl;

    class XMLRPC_DLLEXPORT constrOpt {
    public:
        constrOpt();
        ~constrOpt();

        constrOpt & registryPtr       (xmlrpc_c::registryPtr      const& arg);
        constrOpt & registryP         (const xmlrpc_c::registry * const& arg);
        constrOpt & socketFd          (XMLRPC_SOCKET  const& arg);
        constrOpt & portNumber        (unsigned int   const& arg);
        constrOpt & logFileName       (std::string    const& arg);
        constrOpt & keepaliveTimeout  (unsigned int   const& arg);
        constrOpt & keepaliveMaxConn  (unsigned int   const& arg);
        constrOpt & timeout           (unsigned int   const& arg);
        constrOpt & dontAdvertise     (bool           const& arg);
        constrOpt & uriPath           (std::string    const& arg);
        constrOpt & chunkResponse     (bool           const& arg);
        constrOpt & allowOrigin       (std::string    const& arg);
        constrOpt & accessCtlMaxAge (unsigned int const& arg);
        constrOpt & serverOwnsSignals (bool           const& arg);
        constrOpt & expectSigchld     (bool           const& arg);

    private:
        struct constrOpt_impl * implP;
        friend class serverAbyss;
    };

    serverAbyss(constrOpt const& opt);

    serverAbyss(
        xmlrpc_c::registry const& registry,
        unsigned int       const  portNumber = 8080,
        std::string        const& logFileName = "",
        unsigned int       const  keepaliveTimeout = 0,
        unsigned int       const  keepaliveMaxConn = 0,
        unsigned int       const  timeout = 0,
        bool               const  dontAdvertise = false,
        bool               const  socketBound = false,
        XMLRPC_SOCKET      const  socketFd = 0
        );
    ~serverAbyss();
    
    void
    run();

    void
    runOnce();

    void
    runConn(int const socketFd);

#ifndef WIN32
    void
    sigchld(pid_t pid);
#endif

    void
    terminate();
    
    class XMLRPC_DLLEXPORT shutdown : public xmlrpc_c::registry::shutdown {
    public:
        shutdown(xmlrpc_c::serverAbyss * const severAbyssP);
        virtual ~shutdown();
        void doit(std::string const& comment, void * const callInfo) const;
    private:
        xmlrpc_c::serverAbyss * const serverAbyssP;
    };

private:

    serverAbyss_impl * implP;

    void
    initialize(constrOpt const& opt);
};

class XMLRPC_DLLEXPORT callInfo_serverAbyss : public xmlrpc_c::callInfo {
/*----------------------------------------------------------------------------
   This is information about how an XML-RPC call arrived via an Abyss server.
   It is available to the user's XML-RPC method execute() method, so for
   example an XML-RPC method might execute differently depending upon the
   IP address of the client.

   This is for a user of a xmlrpc_c::serverAbyss server.
-----------------------------------------------------------------------------*/
public:
    callInfo_serverAbyss(xmlrpc_c::serverAbyss * const abyssServerP,
                         TSession *              const abyssSessionP);

    xmlrpc_c::serverAbyss * const serverAbyssP;
        // The server that is processing the RPC.
    TSession * const abyssSessionP;
        // The HTTP transaction that embodies the RPC.  You can ask this
        // object things like what the IP address of the client is.
};

class XMLRPC_DLLEXPORT callInfo_abyss : public xmlrpc_c::callInfo {
/*----------------------------------------------------------------------------
   This is information about how an XML-RPC call arrived via an Abyss server.
   It is available to the user's XML-RPC method execute() method, so for
   example an XML-RPC method might execute differently depending upon the
   IP address of the client.

   This is for a user with his own Abyss server, using
   the "set_handlers" routines to make it into an XML-RPC server.
-----------------------------------------------------------------------------*/
public:
    callInfo_abyss(TSession * const abyssSessionP);

    TSession * abyssSessionP;
        // The HTTP transaction that embodies the RPC.  You can ask this
        // object things like what the IP address of the client is.
};

XMLRPC_DLLEXPORT
void
server_abyss_set_handlers(TServer *          const  srvP,
                          xmlrpc_c::registry const& registry,
                          std::string        const& uriPath = "/RPC2");

XMLRPC_DLLEXPORT
void
server_abyss_set_handlers(TServer *                  const  srvP,
                          const xmlrpc_c::registry * const  registryP,
                          std::string                const& uriPath = "/RPC2");

XMLRPC_DLLEXPORT
void
server_abyss_set_handlers(TServer *             const srvP,
                          xmlrpc_c::registryPtr const registryPtr,
                          std::string           const& uriPath = "/RPC2");

} // namespace

#endif
