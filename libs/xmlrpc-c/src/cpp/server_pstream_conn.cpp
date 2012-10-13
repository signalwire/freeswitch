/*=============================================================================
                              server_pstream
===============================================================================

   RPC server based on a very simple byte stream and XML-RPC XML
   (But this is not an XML-RPC server because it doesn't use HTTP).
   
   The protocol we use is the "packet socket" protocol, which
   is an Xmlrpc-c invention.  It is an almost trivial representation of
   a sequence of packets on a byte stream.

   You can create a pstream server from any file descriptor from which
   you can read and write a bidirectional character stream.  Typically,
   it's a TCP socket.  Such a server talks to one client its entire life.

   Some day, we'll also have a version that you create from a "listening"
   socket, which can talk to multiple clients serially (a client connects,
   does some RPCs, and disconnects).

   By Bryan Henderson 07.05.12.

   Contributed to the public domain by its author.
=============================================================================*/

#include <memory>

#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;
#include "xmlrpc-c/packetsocket.hpp"

#include "xmlrpc-c/server_pstream.hpp"

using namespace std;

namespace xmlrpc_c {


struct serverPstreamConn::constrOpt_impl {

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



serverPstreamConn::constrOpt_impl::constrOpt_impl() {

    this->present.socketFd    = false;
    this->present.registryP   = false;
    this->present.registryPtr = false;
}



serverPstreamConn::constrOpt::constrOpt() {

    this->implP = new constrOpt_impl();
}



serverPstreamConn::constrOpt::~constrOpt() {

    delete(this->implP);
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
serverPstreamConn::constrOpt & \
serverPstreamConn::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->implP->value.OPTION_NAME = arg; \
    this->implP->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(socketFd,    XMLRPC_SOCKET);
DEFINE_OPTION_SETTER(registryP,   const registry *);
DEFINE_OPTION_SETTER(registryPtr, xmlrpc_c::registryPtr);

#undef DEFINE_OPTION_SETTER



struct serverPstreamConn_impl {

    serverPstreamConn_impl(serverPstreamConn::constrOpt_impl const& opt);

    ~serverPstreamConn_impl();

    void
    establishRegistry(serverPstreamConn::constrOpt_impl const& opt);

    void
    establishPacketSocket(serverPstreamConn::constrOpt_impl const& opt);
    
    void
    processRecdPacket(packetPtr  const callPacketP,
                      callInfo * const callInfoP);

    // 'registryP' is what we actually use; 'registryHolder' just holds a
    // reference to 'registryP' so the registry doesn't disappear while
    // this server exists.  But note that if the creator doesn't supply
    // a registryPtr, 'registryHolder' is just a placeholder variable and
    // the creator is responsible for making sure the registry doesn't
    // go anywhere while the server exists.

    registryPtr registryHolder;
    const registry * registryP;

    packetSocket * packetSocketP;
        // The packet socket over which we received RPCs.
        // This is permanently connected to our fixed client.
};



serverPstreamConn_impl::serverPstreamConn_impl(
    serverPstreamConn::constrOpt_impl const& opt) {

    this->establishRegistry(opt);

    this->establishPacketSocket(opt);
}



serverPstreamConn_impl::~serverPstreamConn_impl() {

    delete(this->packetSocketP);
}



void
serverPstreamConn_impl::establishRegistry(
    serverPstreamConn::constrOpt_impl const& opt) {

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



void
serverPstreamConn_impl::establishPacketSocket(
    serverPstreamConn::constrOpt_impl const& opt) {

    if (!opt.present.socketFd)
        throwf("You must provide a 'socketFd' constructor option.");

    auto_ptr<packetSocket> packetSocketAP;

    try {
        auto_ptr<packetSocket> p(new packetSocket(opt.value.socketFd));
        packetSocketAP = p;
    } catch (exception const& e) {
        throwf("Unable to create packet socket out of file descriptor %d.  %s",
               opt.value.socketFd, e.what());
    }
    this->packetSocketP = packetSocketAP.get();
    packetSocketAP.release();
}



serverPstreamConn::serverPstreamConn(constrOpt const& opt) {

    this->implP = new serverPstreamConn_impl(*opt.implP);
}



serverPstreamConn::~serverPstreamConn() {

    delete(this->implP);
}



static void
processCall(const registry * const  registryP,
            packetPtr        const& callPacketP,
            callInfo *       const  callInfoP,
            packetPtr *      const  responsePacketPP) {

    string const callXml(reinterpret_cast<char *>(callPacketP->getBytes()),
                         callPacketP->getLength());

    string responseXml;

    registryP->processCall(callXml, callInfoP, &responseXml);

    *responsePacketPP = packetPtr(new packet(responseXml.c_str(),
                                             responseXml.length()));
}



void
serverPstreamConn_impl::processRecdPacket(packetPtr  const callPacketP,
                                          callInfo * const callInfoP) {
    
    packetPtr responsePacketP;
    try {
        processCall(this->registryP, callPacketP, callInfoP, &responsePacketP);
    } catch (exception const& e) {
        throwf("Error executing received packet as an XML-RPC RPC.  %s",
               e.what());
    }
    try {
        this->packetSocketP->writeWait(responsePacketP);
    } catch (exception const& e) {
        throwf("Failed to write the response to the packet socket.  %s",
               e.what());
    }
}



void
serverPstreamConn::runOnce(callInfo *           const callInfoP,
                           volatile const int * const interruptP,
                           bool *               const eofP) {
/*----------------------------------------------------------------------------
   Get and execute one RPC from the client.

   Unless *interruptP gets set nonzero first.
-----------------------------------------------------------------------------*/
    bool gotPacket;
    packetPtr callPacketP;

    try {
        this->implP->packetSocketP->readWait(interruptP, eofP, &gotPacket,
                                             &callPacketP);
    } catch (exception const& e) {
        throwf("Error reading a packet from the packet socket.  %s",
               e.what());
    }
    if (gotPacket)
        this->implP->processRecdPacket(callPacketP, callInfoP);
}



void
serverPstreamConn::runOnce(volatile const int * const interruptP,
                           bool *               const eofP) {

    this->runOnce(NULL, interruptP, eofP);
}



void
serverPstreamConn::runOnce(bool * const eofP) {
/*----------------------------------------------------------------------------
   Get and execute one RPC from the client.
-----------------------------------------------------------------------------*/
    int const interrupt(0);  // Never interrupt

    this->runOnce(&interrupt, eofP);
}



void
serverPstreamConn::runOnceNoWait(callInfo * const callInfoP,
                                 bool *     const eofP,
                                 bool *     const didOneP) {
/*----------------------------------------------------------------------------
   Get and execute one RPC from the client, unless none has been
   received yet.  Return as *didOneP whether or not one has been
   received.  Unless didOneP is NULL.
-----------------------------------------------------------------------------*/
    bool gotPacket;
    packetPtr callPacketP;

    try {
        this->implP->packetSocketP->read(eofP, &gotPacket, &callPacketP);
    } catch (exception const& e) {
        throwf("Error reading a packet from the packet socket.  %s",
               e.what());
    }
    if (gotPacket)
        this->implP->processRecdPacket(callPacketP, callInfoP);

    if (didOneP)
        *didOneP = gotPacket;
}



void
serverPstreamConn::runOnceNoWait(bool * const eofP,
                                 bool * const didOneP) {

    this->runOnceNoWait(NULL, eofP, didOneP);
}



void
serverPstreamConn::runOnceNoWait(bool * const eofP) {
/*----------------------------------------------------------------------------
   Get and execute one RPC from the client, unless none has been
   received yet.
-----------------------------------------------------------------------------*/
    this->runOnceNoWait(eofP, NULL);
}



void
serverPstreamConn::run(callInfo *           const callInfoP,
                       volatile const int * const interruptP) {

    for (bool clientHasDisconnected = false;
         !clientHasDisconnected && !*interruptP;)
        this->runOnce(callInfoP, interruptP, &clientHasDisconnected);
}



void
serverPstreamConn::run(volatile const int * const interruptP) {

    this->run(NULL, interruptP);
}



void
serverPstreamConn::run() {

    int const interrupt(0);  // Never interrupt

    this->run(&interrupt);
}



} // namespace
