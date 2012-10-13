/*============================================================================
                         packetsocket
==============================================================================

  This is a facility for communicating socket-style, with defined
  packets like a datagram socket but with reliable delivery like a
  stream socket.  It's like a POSIX "sequential packet" socket, except
  it is built on top of a stream socket, so it is usable on the many
  systems that have stream sockets but not sequential packet sockets.

  By Bryan Henderson 2007.05.12

  Contributed to the public domain by its author.
============================================================================*/


/*============================================================================
  The protocol for carrying packets on a character stream:

  The protocol consists of the actual bytes to be transported with a bare
  minimum of framing information added:

     An ASCII Escape (<ESC> == 0x1B) character marks the start of a
     4-ASCII-character control word.  These are defined:

       <ESC>PKT : marks the beginning of a packet.
       <ESC>END : marks the end of a packet.
       <ESC>ESC : represents an <ESC> character in the packet
       <ESC>NOP : no meaning

     Any other bytes after <ESC> is a protocol error.

     A stream is all the data transmitted during a single socket
     connection.

     End of stream in the middle of a packet is a protocol error.  

     All bytes not part of a control word are literal bytes of a packet.

  You can create a packet socket from a POSIX stream socket or a
  Windows emulation of one.

  One use of the NOP control word is to validate that the connection
  is still working.  You might send one periodically to detect, for
  example, an unplugged TCP/IP network cable.  It's probably better
  to use the TCP keepalive facility for that.
============================================================================*/
#include "xmlrpc_config.h"

#include <cassert>
#include <string>
#include <queue>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>

#include <errno.h>
#include <fcntl.h>
#if MSVCRT
# include <winsock2.h>
# include <io.h>
#else
# include <unistd.h>
# include <poll.h>
# include <sys/socket.h>
#endif

#include <sys/types.h>

#include "c_util.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;

#include "xmlrpc-c/packetsocket.hpp"

using namespace std;

#define ESC 0x1B   //  ASCII Escape character
#define ESC_STR "\x1B"

class XMLRPC_DLLEXPORT socketx {

public:
    socketx(int const sockFd);

    ~socketx();

    void
    waitForReadable() const;

    void
    waitForWritable() const;

    void
    read(unsigned char * const buffer,
         size_t          const bufferSize,
         bool *          const wouldblockP,
         size_t *        const bytesReadP) const;

    void
    writeWait(const unsigned char * const data,
              size_t                const size) const;
private:
    int fd;
    bool fdIsBorrowed;
};



/* Sockets are similar, but not identical between Unix and Windows.
   Some Unix socket functions appear to be available on Windows (a
   Unix compatibility feature), but work only for file descriptor
   numbers < 32, so we don't use those.
*/

socketx::socketx(int const sockFd) {
#if MSVCRT        
    // We don't have any way to duplicate; we'll just have to borrow.
    this->fdIsBorrowed = true;
    this->fd = sockFd;
    u_long iMode(1);  // Nonblocking mode yes
    ioctlsocket(this->fd, FIONBIO, &iMode);  // Make socket nonblocking
#else
    this->fdIsBorrowed = false;

    int dupRc;

    dupRc = dup(sockFd);

    if (dupRc < 0)
        throwf("dup() failed.  errno=%d (%s)", errno, strerror(errno));
    else {
        this->fd = dupRc;
        fcntl(this->fd, F_SETFL, O_NONBLOCK);  // Make socket nonblocking
    }
#endif
}



socketx::~socketx() {

    if (!this->fdIsBorrowed) {
#if MSVCRT
        ::closesocket(SOCKET(this->fd));
#else
        close(this->fd);
#endif
    }
}



void
socketx::waitForReadable() const {
    /* Return when there is something to read from the socket
       (an EOF indication counts as something to read).  Also
       return if there is a signal (handled, of course).  Rarely,
       it is OK to return when there isn't anything to read.
    */
#if  MSVCRT
    // poll() is not available; settle for select().
    // Starting in Windows Vista, there is WSApoll()
    fd_set rd_set;
    FD_ZERO(&rd_set);
    FD_SET(this->fd, &rd_set);

    select(this->fd + 1, &rd_set, 0, 0, 0);
#else
    // poll() beats select() because higher file descriptor numbers
    // work.
    struct pollfd pollfds[1];

    pollfds[0].fd = this->fd;
    pollfds[0].events = POLLIN;

    poll(pollfds, ARRAY_SIZE(pollfds), -1);
#endif
}



void
socketx::waitForWritable() const {
    /* Return when socket is able to be written to. */
#if MSVCRT
    fd_set wr_set;
    FD_ZERO(&wr_set);
    FD_SET(this->fd, &wr_set);

    select(this->fd + 1, 0, &wr_set, 0, 0);
#else
    struct pollfd pollfds[1];
        
    pollfds[0].fd = this->fd;
    pollfds[0].events = POLLOUT;
        
    poll(pollfds, ARRAY_SIZE(pollfds), -1);
#endif
}



static bool
wouldBlock() {
/*----------------------------------------------------------------------------
   The most recently executed system socket function, which we assume failed,
   failed because the situation was such that it wanted to block, but the
   socket had the nonblocking option.
-----------------------------------------------------------------------------*/
#if MSVCRT
    return (WSAGetLastError() == WSAEWOULDBLOCK ||
            WSAGetLastError() == WSAEINPROGRESS);
#else
    /* EWOULDBLOCK and EAGAIN are normally synonyms, but POSIX allows them
       to be separate and allows the OS to return whichever one it wants
       for the "would block" condition.
    */
    return (errno == EWOULDBLOCK || errno == EAGAIN);
#endif
}



static string
lastErrorDesc() {
/*----------------------------------------------------------------------------
   A description suitable for an error message of why the most recent
   failed system socket function failed.
-----------------------------------------------------------------------------*/
    ostringstream msg;
#if MSVCRT
    int const lastError = WSAGetLastError();
    msg << "winsock error code " << lastError << " "
        << "(" << strerror(lastError) << ")";
#else
    msg << "errno = " << errno << ", (" << strerror(errno);
#endif
    return msg.str();
}




void
socketx::read(unsigned char * const buffer,
              size_t          const bufferSize,
              bool *          const wouldblockP,
              size_t *        const bytesReadP) const {
    
    int rc;

    // We've seen a Windows library whose recv() expects a char * buffer
    // (cf POSIX void *), so we cast.

    rc = recv(this->fd, (char *)buffer, bufferSize, 0);

    if (rc < 0) {
        if (wouldBlock()) {
            *wouldblockP = true;
            *bytesReadP  = 0;
        } else
            throwf("read() of socket failed with %s", lastErrorDesc().c_str());
    } else {
        *wouldblockP = false;
        *bytesReadP  = rc;
    }
}



static void
writeFd(int                   const fd,
        const unsigned char * const data,
        size_t                const size,
        size_t *              const bytesWrittenP) {

    size_t totalBytesWritten;
    bool full;  // File image is "full" for now - won't take any more data

    full = false;
    totalBytesWritten = 0;

    while (totalBytesWritten < size && !full) {
        int rc;

        rc = send(fd, (char*)&data[totalBytesWritten],
                  size - totalBytesWritten, 0);

        if (rc < 0) {
            if (wouldBlock())
                full = true;
            else
                throwf("write() of socket failed with %s",
                       lastErrorDesc().c_str());
        } else if (rc == 0)
            throwf("Zero byte short write.");
        else {
            size_t const bytesWritten(rc);
            totalBytesWritten += bytesWritten;
        }
    }
    *bytesWrittenP = totalBytesWritten;
}



void
socketx::writeWait(const unsigned char * const data,
                   size_t                const size) const {
/*----------------------------------------------------------------------------
   Write the 'size' bytes at 'data' to the socket.  Wait as long
   as it takes for the file image to be able to take all the data.
-----------------------------------------------------------------------------*/
    size_t totalBytesWritten;

    // We do the first one blind because it will probably just work
    // and we don't want to waste the poll() call and buffer arithmetic.

    writeFd(this->fd, data, size, &totalBytesWritten);

    while (totalBytesWritten < size) {
        this->waitForWritable();

        size_t bytesWritten;

        writeFd(this->fd, &data[totalBytesWritten], size - totalBytesWritten,
                &bytesWritten);

        totalBytesWritten += bytesWritten;
    }
}



namespace xmlrpc_c {


packet::packet() :
    bytes(NULL), length(0), allocSize(0) {}
 


void
packet::initialize(const unsigned char * const data,
                   size_t                const dataLength) {

    this->bytes = reinterpret_cast<unsigned char *>(malloc(dataLength));

    if (this->bytes == NULL)
        throwf("Can't get storage for a %u-byte packet", (unsigned)dataLength);

    this->allocSize = dataLength;

    memcpy(this->bytes, data, dataLength);

    this->length = dataLength;
}



packet::packet(const unsigned char * const data,
               size_t                const dataLength) {

    this->initialize(data, dataLength);
}



packet::packet(const char * const data,
               size_t       const dataLength) {

    this->initialize(reinterpret_cast<const unsigned char *>(data),
                     dataLength);
}



packet::~packet() {

    if (this->bytes)
        free(bytes);
}



void
packet::addData(const unsigned char * const data,
                size_t                const dataLength) {
/*----------------------------------------------------------------------------
   Add the 'length' bytes at 'data' to the packet.

   We allocate whatever additional memory is needed to fit the new
   data in.
-----------------------------------------------------------------------------*/
    size_t const neededSize(this->length + dataLength);

    if (this->allocSize < neededSize)
        this->bytes = reinterpret_cast<unsigned char *>(
            realloc(this->bytes, neededSize));

    if (this->bytes == NULL)
        throwf("Can't get storage for a %u-byte packet", (unsigned)neededSize);

    memcpy(this->bytes + this->length, data, dataLength);

    this->length += dataLength;
}



packetPtr::packetPtr() {
    // Base class constructor will construct pointer that points to nothing
}



packetPtr::packetPtr(packet * const packetP) : autoObjectPtr(packetP) {}



packet *
packetPtr::operator->() const {

    girmem::autoObject * const p(this->objectP);
    return dynamic_cast<packet *>(p);
}



class packetSocket_impl {

public:
    packetSocket_impl(int const sockFd);

    void
    writeWait(packetPtr const& packetP) const;

    void
    read(bool *      const eofP,
         bool *      const gotPacketP,
         packetPtr * const packetPP);

    void
    readWait(volatile const int * const interruptP,
             bool *               const eofP,
             bool *               const gotPacketP,
             packetPtr *          const packetPP);

private:
    socketx sock;
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



packetSocket_impl::packetSocket_impl(int const sockFd) :
    sock(sockFd) {

    this->inEscapeSeq  = false;
    this->inPacket     = false;
    this->escAccum.len = 0;
    this->eof          = false;
}



/*----------------------------------------------------------------------------
   To complete the job, we should provide writing services analogous
   to the reading services.  That means a no-wait write method and
   the ability to interrupt with a signal without corrupting the write
   stream.

   We're a little to lazy to do that now, since we don't need it yet,
   but here's a design for that:

   The packetSocket has a send queue of packets called the write
   buffer.  It stores packetPtr pointers to packets created by the
   user.

   packetSocket::write() adds a packet to the write buffer, then calls
   packetSocket::writeToFile().  If you give it a null packetPtr,
   it just calls writeToFile().

   packetSocket::writeToFile() writes from the write buffer to the
   socket whatever the socket will take immediately.  It writes the
   start sequence, writes the packet data, then writes the end
   sequence.  The packetSocket keeps track of where it is in the
   process of writing the current send packet (including start end
   end sequences) it is.

   packetSocket::write() returns a "flushed" flag indicating that there
   is nothing left in the write buffer.

   packetSocket::writeWait() just calls packetSocket::write(), then
   packetSocket::flush() in a poll() loop.
-----------------------------------------------------------------------------*/


void
packetSocket_impl::writeWait(packetPtr const& packetP) const {

    const unsigned char * const packetStart(
        reinterpret_cast<const unsigned char *>(ESC_STR "PKT"));
    const unsigned char * const packetEnd(
        reinterpret_cast<const unsigned char *>(ESC_STR "END"));

    this->sock.writeWait(packetStart, 4);

    this->sock.writeWait(packetP->getBytes(), packetP->getLength());

    this->sock.writeWait(packetEnd, 4);
}



void
packetSocket_impl::takeSomeEscapeSeq(const unsigned char * const buffer,
                                     size_t                const length,
                                     size_t *              const bytesTakenP) {
/*----------------------------------------------------------------------------
   Take and process some bytes from the incoming stream 'buffer',
   which contains 'length' bytes, assuming they are within an escape
   sequence.
-----------------------------------------------------------------------------*/
    size_t bytesTaken;

    bytesTaken = 0;

    while (this->escAccum.len < 3 && bytesTaken < length)
        this->escAccum.bytes[this->escAccum.len++] = buffer[bytesTaken++];

    assert(this->escAccum.len <= 3);

    if (this->escAccum.len == 3) {
        if (0) {
        } else if (xmlrpc_memeq(this->escAccum.bytes, "NOP", 3)) {
            // Nothing to do
        } else if (xmlrpc_memeq(this->escAccum.bytes, "PKT", 3)) {
            this->packetAccumP = packetPtr(new packet);
            this->inPacket = true;
        } else if (xmlrpc_memeq(this->escAccum.bytes, "END", 3)) {
            if (this->inPacket) {
                this->readBuffer.push(this->packetAccumP);
                this->inPacket = false;
                this->packetAccumP = packetPtr();
            } else
                throwf("END control word received without preceding PKT");
        } else if (xmlrpc_memeq(this->escAccum.bytes, "ESC", 3)) {
            if (this->inPacket)
                this->packetAccumP->addData((const unsigned char *)ESC_STR, 1);
            else
                throwf("ESC control work received outside of a packet");
        } else
            throwf("Invalid escape sequence 0x%02x%02x%02x read from "
                   "stream socket under packet socket",
                   this->escAccum.bytes[0],
                   this->escAccum.bytes[1],
                   this->escAccum.bytes[2]);
        
        this->inEscapeSeq = false;
        this->escAccum.len = 0;
    }
    *bytesTakenP = bytesTaken;
}



void
packetSocket_impl::takeSomePacket(const unsigned char * const buffer,
                                  size_t                const length,
                                  size_t *              const bytesTakenP) {

    assert(!this->inEscapeSeq);

    const unsigned char * const escPos(
        (const unsigned char *)memchr(buffer, ESC, length));

    if (escPos) {
        size_t const escOffset(escPos - &buffer[0]);
        // move everything before the escape sequence into the
        // packet accumulator.
        this->packetAccumP->addData(buffer, escOffset);

        // Caller can pick up from here; we don't know nothin' 'bout
        // no escape sequences.

        *bytesTakenP = escOffset;
    } else {
        // No complete packet yet and no substitution to do;
        // just throw the whole thing into the accumulator.
        this->packetAccumP->addData(buffer, length);
        *bytesTakenP = length;
    }
}



void
packetSocket_impl::verifyNothingAccumulated() {
/*----------------------------------------------------------------------------
   Throw an error if there is a partial packet accumulated.
-----------------------------------------------------------------------------*/
    if (this->inEscapeSeq)
        throwf("Streams socket closed in the middle of an "
               "escape sequence");
    
    if (this->inPacket)
        throwf("Stream socket closed in the middle of a packet "
               "(%u bytes of packet received; no END marker to mark "
               "end of packet)", (unsigned)this->packetAccumP->getLength());
}



void
packetSocket_impl::processBytesRead(const unsigned char * const buffer,
                                    size_t                const bytesRead) {

    unsigned int cursor;  // Cursor into buffer[]
    cursor = 0;
    while (cursor < bytesRead) {
        size_t bytesTaken;

        if (this->inEscapeSeq)
            this->takeSomeEscapeSeq(&buffer[cursor],
                                    bytesRead - cursor,
                                    &bytesTaken);
        else if (buffer[cursor] == ESC) {
            this->inEscapeSeq = true;
            bytesTaken = 1;
        } else if (this->inPacket)
            this->takeSomePacket(&buffer[cursor],
                                 bytesRead - cursor,
                                 &bytesTaken);
        else
            throwf("Byte 0x%02x is not in a packet or escape sequence.  "
                   "Sender is probably not using packet socket protocol",
                   buffer[cursor]);
        
        cursor += bytesTaken;
    }
}



void
packetSocket_impl::readFromFile() {
/*----------------------------------------------------------------------------
   Read some data from the underlying stream socket.  Read as much as is
   available right now, up to 4K.  Update 'this' to reflect the data read.

   E.g. if we read an entire packet, we add it to the packet buffer
   (this->readBuffer).  If we read the first part of a packet, we add
   it to the packet accumulator (*this->packetAccumP).  If we read the end
   of a packet, we add the full packet to the packet buffer and empty
   the packet accumulator.  Etc.
-----------------------------------------------------------------------------*/
    bool wouldblock;

    wouldblock = false;

    while (this->readBuffer.empty() && !this->eof && !wouldblock) {
        unsigned char buffer[4096];
        size_t bytesRead;

        this->sock.read(buffer, sizeof(buffer), &wouldblock, &bytesRead);

        if (!wouldblock) {
            if (bytesRead == 0) {
                this->eof = true;
                this->verifyNothingAccumulated();
            } else
                this->processBytesRead(buffer, bytesRead);
        }
    }
}



void
packetSocket_impl::read(bool *      const eofP,
                        bool *      const gotPacketP,
                        packetPtr * const packetPP) {
/*----------------------------------------------------------------------------
   Read one packet from the socket, through the internal packet buffer.

   If there is a packet immediately available, return it as *packetPP and
   return *gotPacketP true.  Otherwise, return *gotPacketP false.

   Iff the socket has no more data coming (it is shut down) and there
   is no complete packet in the packet buffer, return *eofP.

   This leaves one other possibility: there is no full packet immediately
   available, but there may be in the future because the socket is still
   alive.  In that case, we return *eofP == false and *gotPacketP == false.

   Any packet we return belongs to caller; Caller must delete it.
-----------------------------------------------------------------------------*/
    // Move any packets now waiting to be read in the underlying stream
    // socket into our packet buffer (this->readBuffer).

    this->readFromFile();

    if (this->readBuffer.empty()) {
        *gotPacketP = false;
        *eofP = this->eof;
    } else {
        *gotPacketP = true;
        *eofP = false;
        *packetPP = this->readBuffer.front();
        readBuffer.pop();
    }
}



void
packetSocket_impl::readWait(volatile const int * const interruptP,
                            bool *               const eofP,
                            bool *               const gotPacketP,
                            packetPtr *          const packetPP) {

    bool gotPacket;
    bool eof;

    gotPacket = false;
    eof = false;

    while (!gotPacket && !eof && !*interruptP) {

        this->sock.waitForReadable();
        this->read(&eof, &gotPacket, packetPP);
    }

    *gotPacketP = gotPacket;
    *eofP = eof;
}



packetSocket::packetSocket(int const sockFd) {

    this->implP = new packetSocket_impl(sockFd);
}



packetSocket::~packetSocket() {

    delete(this->implP);
}



void
packetSocket::writeWait(packetPtr const& packetP) const {

    implP->writeWait(packetP);
}



void
packetSocket::read(bool *      const eofP,
                   bool *      const gotPacketP,
                   packetPtr * const packetPP) {

    this->implP->read(eofP, gotPacketP, packetPP);
}



void
packetSocket::readWait(volatile const int * const interruptP,
                       bool *               const eofP,
                       bool *               const gotPacketP,
                       packetPtr *          const packetPP) {

    this->implP->readWait(interruptP, eofP, gotPacketP, packetPP);
}



void
packetSocket::readWait(volatile const int * const interruptP,
                       bool *               const eofP,
                       packetPtr *          const packetPP) {

    bool gotPacket;

    this->implP->readWait(interruptP, eofP, &gotPacket, packetPP);

    if (!gotPacket)
        throwf("Packet read was interrupted");
}



void
packetSocket::readWait(bool *      const eofP,
                       packetPtr * const packetPP) {

    int const interrupt(0);  // Never interrupt

    this->readWait(&interrupt, eofP, packetPP);
}

} // namespace
