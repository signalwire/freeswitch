#ifndef PACKETSOCKET_HPP_INCLUDED
#define PACKETSOCKET_HPP_INCLUDED

/*============================================================================
                         packetsocket
==============================================================================
  This is a facility for communicating socket-style, with defined
  packets like a datagram socket but with reliable delivery like a
  stream socket.  It's like a POSIX "sequential packet" socket, except
  it is built on top of a stream socket, so it is usable on the many
  systems that have stream sockets but not sequential packet sockets.
============================================================================*/

#include <sys/types.h>
#include <string>
#include <queue>

#include <xmlrpc-c/girmem.hpp>

namespace xmlrpc_c {

class packet : public girmem::autoObject {

public:
    packet();

    packet(const unsigned char * const data,
                   size_t                const dataLength);

    packet(const char * const data,
                   size_t       const dataLength);

    ~packet();

    unsigned char *
    getBytes() const { return this->bytes; }

    size_t
    getLength() const { return this->length; }

    void
    addData(const unsigned char * const data,
            size_t                const dataLength);

private:
    unsigned char * bytes;  // malloc'ed
    size_t length;
    size_t allocSize;

    void
    initialize(const unsigned char * const data,
               size_t                const dataLength);
};



class packetPtr: public girmem::autoObjectPtr {

public:
    packetPtr();

    explicit packetPtr(packet * const packetP);

    packet *
    operator->() const;
};



class packetSocket {
/*----------------------------------------------------------------------------
   This is an Internet communication vehicle that transmits individual
   variable-length packets of text.

   It is based on a stream socket.

   It would be much better to use a kernel SOCK_SEQPACKET socket, but
   Linux 2.4 does not have them.
-----------------------------------------------------------------------------*/
public:
    packetSocket(int sockFd);

    ~packetSocket();

    void
    writeWait(packetPtr const& packetPtr) const;

    void
    read(bool *      const eofP,
         bool *      const gotPacketP,
         packetPtr * const packetPP);

    void
    readWait(volatile const int * const interruptP,
             bool *               const eofP,
             bool *               const gotPacketP,
             packetPtr *          const packetPP);

    void
    readWait(volatile const int * const interruptP,
             bool *               const eofP,
             packetPtr *          const packetPP);

    void
    readWait(bool *      const eofP,
             packetPtr * const packetPP);

private:
    int sockFd;
        // The kernel stream socket we use.
    bool eof;
        // The packet socket is at end-of-file for reads.
        // 'readBuffer' is empty and there won't be any more data to fill
        // it because the underlying stream socket is closed.
    std::queue<packetPtr> readBuffer;
    packetPtr packetAccumP;
        // The receive packet we're currently accumulating; it will join
        // 'readBuffer' when we've received the whole packet (and we've
        // seen the END escape sequence so we know we've received it all).
        // If we're not currently accumulating a packet (haven't seen a
        // PKT escape sequence), this points to nothing.
    bool inEscapeSeq;
        // In our trek through the data read from the underlying stream
        // socket, we are after an ESC character and before the end of the
        // escape sequence.  'escAccum' shows what of the escape sequence
        // we've seen so far.
    bool inPacket;
        // We're now receiving packet data from the underlying stream
        // socket.  We've seen a complete PKT escape sequence, but have not
        // seen a complete END escape sequence since.
    struct {
        unsigned char bytes[3];
        size_t len;
    } escAccum;

    void
    bufferFinishedPacket();

    void
    takeSomeEscapeSeq(const unsigned char * const buffer,
                                    size_t                const length,
                                    size_t *              const bytesTakenP);

    void
    takeSomePacket(const unsigned char * const buffer,
                   size_t                const length,
                   size_t *              const bytesTakenP);

    void
    verifyNothingAccumulated();

    void
    processBytesRead(const unsigned char * const buffer,
                     size_t                const bytesRead);

    void
    readFromFile();

};



} // namespace

#endif
