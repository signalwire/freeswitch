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


serverPstreamConn::constrOpt::constrOpt() {

    present.socketFd    = false;
    present.registryP   = false;
    present.registryPtr = false;
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
serverPstreamConn::constrOpt & \
serverPstreamConn::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->value.OPTION_NAME = arg; \
    this->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(socketFd,    XMLRPC_SOCKET);
DEFINE_OPTION_SETTER(registryP,   const registry *);
DEFINE_OPTION_SETTER(registryPtr, xmlrpc_c::registryPtr);

#undef DEFINE_OPTION_SETTER



void
serverPstreamConn::establishRegistry(constrOpt const& opt) {

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
serverPstreamConn::establishPacketSocket(constrOpt const& opt) {

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

    this->establishRegistry(opt);

    this->establishPacketSocket(opt);
}



serverPstreamConn::~serverPstreamConn() {

    delete(this->packetSocketP);
}



void
processCall(const registry * const  registryP,
            packetPtr        const& callPacketP,
            packetPtr *      const  responsePacketPP) {

    string const callXml(reinterpret_cast<char *>(callPacketP->getBytes()),
                         callPacketP->getLength());

    string responseXml;

    registryP->processCall(callXml, &responseXml);

    *responsePacketPP = packetPtr(new packet(responseXml.c_str(),
                                             responseXml.length()));
}



void
serverPstreamConn::runOnce(volatile const int * const interruptP,
                           bool *               const eofP) {
/*----------------------------------------------------------------------------
   Get and execute one RPC from the client.

   Unless *interruptP gets set nonzero first.
-----------------------------------------------------------------------------*/
    bool gotPacket;
    packetPtr callPacketP;

    try {
        this->packetSocketP->readWait(interruptP, eofP, &gotPacket,
                                      &callPacketP);
    } catch (exception const& e) {
        throwf("Error reading a packet from the packet socket.  %s",
               e.what());
    }
    if (gotPacket) {
        packetPtr responsePacketP;
        try {
            processCall(this->registryP, callPacketP, &responsePacketP);
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
}



void
serverPstreamConn::runOnce(bool * const eofP) {
/*----------------------------------------------------------------------------
   Get and execute one RPC from the client.
-----------------------------------------------------------------------------*/
    int const interrupt(0);  // Never interrupt

    this->runOnce(&interrupt, eofP);
}



} // namespace
