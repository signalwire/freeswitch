/*=============================================================================
                              pstream
===============================================================================

   Client XML transport for Xmlrpc-c based on a very simple byte
   stream.
   
   The protocol we use is the "packet socket" protocol, which
   is an Xmlrpc-c invention.  It is an almost trivial representation of
   a sequence of packets on a byte stream.

   A transport object talks to exactly one server over its lifetime.

   You can create a pstream transport from any file descriptor from which
   you can read and write a bidirectional character stream.  Typically,
   it's a TCP socket.

   This transport is synchronous only.  It does not provide a working
   'start' method.  You have at most one outstanding RPC and wait for
   it to complete.

   By Bryan Henderson 07.05.12.

   Contributed to the public domain by its author.
=============================================================================*/

#include <memory>

#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;
#include "xmlrpc-c/packetsocket.hpp"

#include "xmlrpc-c/client_transport.hpp"

using namespace std;

namespace xmlrpc_c {


clientXmlTransport_pstream::constrOpt::constrOpt() {

    present.fd = false;
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
clientXmlTransport_pstream::constrOpt & \
clientXmlTransport_pstream::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->value.OPTION_NAME = arg; \
    this->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(fd, xmlrpc_socket);

#undef DEFINE_OPTION_SETTER



clientXmlTransport_pstream::clientXmlTransport_pstream(constrOpt const& opt) {

    if (!opt.present.fd)
        throwf("You must provide a 'fd' constructor option.");

    auto_ptr<packetSocket> packetSocketAP;

    try {
        auto_ptr<packetSocket> p(new packetSocket(opt.value.fd));
        packetSocketAP = p;
    } catch (exception const& e) {
        throwf("Unable to create packet socket out of file descriptor %d.  %s",
               opt.value.fd, e.what());
    }
    this->packetSocketP = packetSocketAP.get();
    packetSocketAP.release();
}



clientXmlTransport_pstream::~clientXmlTransport_pstream() {

    delete(this->packetSocketP);
}


void
clientXmlTransport_pstream::call(
    carriageParm * const  carriageParmP,
    string         const& callXml,
    string *       const  responseXmlP) {

    carriageParm_pstream * const carriageParmPstreamP(
        dynamic_cast<carriageParm_pstream *>(carriageParmP));

    if (carriageParmPstreamP == NULL)
        throwf("Pstream client XML transport called with carriage "
               "parameter object not of class carriageParm_pstream");

    packetPtr const callPacketP(new packet(callXml.c_str(), callXml.length()));

    try {
        this->packetSocketP->writeWait(callPacketP);
    } catch (exception const& e) {
        throwf("Failed to write the call to the packet socket.  %s", e.what());
    }
    packetPtr responsePacketP;

    try {
        bool eof;
        this->packetSocketP->readWait(&eof, &responsePacketP);

        if (eof)
            throwf("The other end closed the socket before sending "
                   "the response.");
    } catch (exception const& e) {
        throwf("We sent the call, but couldn't get the response.  %s",
               e.what());
    }
    *responseXmlP = 
        string(reinterpret_cast<char *>(responsePacketP->getBytes()),
               responsePacketP->getLength());
}



} // namespace
