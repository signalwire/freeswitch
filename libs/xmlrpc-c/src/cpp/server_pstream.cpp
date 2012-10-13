/*=============================================================================
                              server_pstream
===============================================================================

   RPC server based on a very simple byte stream and XML-RPC XML
   (But this is not an XML-RPC server because it doesn't use HTTP).
   
   The protocol we use is the "packet socket" protocol, which
   is an Xmlrpc-c invention.  It is an almost trivial representation of
   a sequence of packets on a byte stream.

   By Bryan Henderson 09.03.22

   Contributed to the public domain by its author.
=============================================================================*/

#include "xmlrpc_config.h"
#if MSVCRT
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <winsock.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#endif
#include <errno.h>
#include <cstring>
#include <memory>

#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;

#include "xmlrpc-c/server_pstream.hpp"

using namespace std;

namespace xmlrpc_c {


struct serverPstream::constrOpt_impl {

    constrOpt_impl();

    struct value {
        xmlrpc_c::registryPtr      registryPtr;
        const xmlrpc_c::registry * registryP;
        XMLRPC_SOCKET              socketFd;
    } value;
    struct {
        bool registryPtr;
        bool registryP;
        bool socketFd;
    } present;
};



serverPstream::constrOpt_impl::constrOpt_impl() {

    this->present.socketFd    = false;
    this->present.registryP   = false;
    this->present.registryPtr = false;
}



serverPstream::constrOpt::constrOpt() {

    this->implP = new serverPstream::constrOpt_impl();
}



serverPstream::constrOpt::~constrOpt() {

    delete(this->implP);
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
serverPstream::constrOpt & \
serverPstream::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->implP->value.OPTION_NAME = arg; \
    this->implP->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(socketFd,    XMLRPC_SOCKET);
DEFINE_OPTION_SETTER(registryP,   const registry *);
DEFINE_OPTION_SETTER(registryPtr, xmlrpc_c::registryPtr);

#undef DEFINE_OPTION_SETTER



struct serverPstream_impl {

    serverPstream_impl(serverPstream::constrOpt_impl const& opt);

    ~serverPstream_impl();

    void
    establishRegistry(serverPstream::constrOpt_impl const& opt);

    // 'registryP' is what we actually use; 'registryHolder' just holds a
    // reference to 'registryP' so the registry doesn't disappear while
    // this server exists.  But note that if the creator doesn't supply
    // a registryPtr, 'registryHolder' is just a placeholder variable and
    // the creator is responsible for making sure the registry doesn't
    // go anywhere while the server exists.

    registryPtr registryHolder;
    const registry * registryP;

    XMLRPC_SOCKET listenSocketFd;
        // The socket on which we accept connections from clients.  This comes
        // to us from the creator, already bound and in listen mode.  That
        // way, this object doesn't have to know anything about socket
        // addresses or listen parameters such as the maximum connection
        // backlog size.
    
    bool termRequested;
        // User has requested that the run method return ASAP; i.e. that
        // the server cease servicing RPCs.
};



serverPstream_impl::serverPstream_impl(
    serverPstream::constrOpt_impl const& opt) {

    this->establishRegistry(opt);

    if (!opt.present.socketFd)
        throwf("You must provide a 'socketFd' constructor option.");
    
    this->listenSocketFd = opt.value.socketFd;

    this->termRequested = false;
}



serverPstream_impl::~serverPstream_impl() {

}



void
serverPstream_impl::establishRegistry(
    serverPstream::constrOpt_impl const& opt) {

    if (!opt.present.registryP && !opt.present.registryPtr)
        throwf("You must specify the 'registryP' or 'registryPtr' option");
    else if (opt.present.registryP && opt.present.registryPtr)
        throwf("You may not specify both the 'registryP' and "
               "the 'registryPtr' options");
    else {
        if (opt.present.registryP)
            this->registryP      = opt.value.registryP;
        else {
            this->registryHolder = opt.value.registryPtr;
            this->registryP      = opt.value.registryPtr.get();
        }
    }
}


/*-----------------------------------------------------------------------------
   serverPstream::shutdown is a derived class of registry::shutdown.  You give
   it to the registry object to allow XML-RPC method 'system.shutdown' to
-----------------------------------------------------------------------------*/

serverPstream::shutdown::shutdown(serverPstream * const serverPstreamP) :
    serverPstreamP(serverPstreamP) {}



serverPstream::shutdown::~shutdown() {}



void
serverPstream::shutdown::doit(string const&,
                              void * const) const {

    this->serverPstreamP->terminate();
}
/*---------------------------------------------------------------------------*/



serverPstream::serverPstream(constrOpt const& opt) {

    this->implP = new serverPstream_impl(*opt.implP);
}



serverPstream::~serverPstream() {

    delete(this->implP);
}



void
serverPstream::runSerial(volatile const int * const interruptP) {

    while (!this->implP->termRequested && !*interruptP) {
        struct sockaddr peerAddr;
        socklen_t size = sizeof(peerAddr);
        int rc;

        rc = accept(this->implP->listenSocketFd, &peerAddr, &size);

        if (!*interruptP) {
            if (rc < 0)
                if (errno == EINTR) {
                    // system call was interrupted, but user doesn't want
                    // to interrupt the server, so just keep trying
                } else
                    throwf("Failed to accept a connection "
                           "on the listening socket.  accept() failed "
                           "with errno %d (%s)", errno, strerror(errno));
            else {
                int const acceptedFd = rc;

                serverPstreamConn connectionServer(
                    xmlrpc_c::serverPstreamConn::constrOpt()
                    .socketFd(acceptedFd)
                    .registryP(this->implP->registryP));

                callInfo_serverPstream callInfo(this, peerAddr, size);

                connectionServer.run(&callInfo, interruptP);
            }
        }
    }
}



void
serverPstream::runSerial() {

    int const interrupt(0);  // Never interrupt

    this->runSerial(&interrupt);
}



void
serverPstream::terminate() {

    this->implP->termRequested = true;
}



callInfo_serverPstream::callInfo_serverPstream(
    serverPstream * const serverP,
    struct sockaddr const clientAddr,
    socklen_t const clientAddrSize) :

    serverP(serverP),
    clientAddr(clientAddr),
    clientAddrSize(clientAddrSize)

{}



} // namespace
