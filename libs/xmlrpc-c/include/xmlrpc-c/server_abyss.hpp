#ifndef SERVER_ABYSS_HPP_INCLUDED
#define SERVER_ABYSS_HPP_INCLUDED

#ifdef WIN32
#include <winsock.h>   // For XMLRPC_SOCKET (= SOCKET)
#endif

#include "xmlrpc-c/config.h"  // For XMLRPC_SOCKET
#include "xmlrpc-c/base.hpp"
#include "abyss.h"

namespace xmlrpc_c {

class serverAbyss {
    
public:
    class constrOpt {
    public:
        constrOpt();

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

        struct value {
            xmlrpc_c::registryPtr      registryPtr;
            const xmlrpc_c::registry * registryP;
            XMLRPC_SOCKET  socketFd;
            unsigned int   portNumber;
            std::string    logFileName;
            unsigned int   keepaliveTimeout;
            unsigned int   keepaliveMaxConn;
            unsigned int   timeout;
            bool           dontAdvertise;
            std::string    uriPath;
            bool           chunkResponse;
        } value;
        struct {
            bool registryPtr;
            bool registryP;
            bool socketFd;
            bool portNumber;
            bool logFileName;
            bool keepaliveTimeout;
            bool keepaliveMaxConn;
            bool timeout;
            bool dontAdvertise;
            bool uriPath;
            bool chunkResponse;
        } present;
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

    void
    terminate();
    
    class shutdown : public xmlrpc_c::registry::shutdown {
    public:
        shutdown(xmlrpc_c::serverAbyss * const severAbyssP);
        virtual ~shutdown();
        void doit(std::string const& comment, void * const callInfo) const;
    private:
        xmlrpc_c::serverAbyss * const serverAbyssP;
    };

private:
    // The user has the choice of supplying the registry by plain pointer
    // (and managing the object's existence himself) or by autoObjectPtr
    // (with automatic management).  'registryPtr' exists here only to
    // maintain a reference count in the case that the user supplied an
    // autoObjectPtr.  The object doesn't reference the C++ registry
    // object except during construction, because the C registry is the
    // real registry.
    xmlrpc_c::registryPtr registryPtr;

    TServer cServer;

    void
    setAdditionalServerParms(constrOpt const& opt);

    void
    initialize(constrOpt const& opt);
};


void
server_abyss_set_handlers(TServer *          const  srvP,
                          xmlrpc_c::registry const& registry,
                          std::string        const& uriPath = "/RPC2");

void
server_abyss_set_handlers(TServer *                  const  srvP,
                          const xmlrpc_c::registry * const  registryP,
                          std::string                const& uriPath = "/RPC2");

void
server_abyss_set_handlers(TServer *             const srvP,
                          xmlrpc_c::registryPtr const registryPtr,
                          std::string           const& uriPath = "/RPC2");

} // namespace

#endif
