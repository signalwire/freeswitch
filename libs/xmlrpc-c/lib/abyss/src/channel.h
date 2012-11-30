#ifndef CHANNEL_H_INCLUDED
#define CHANNEL_H_INCLUDED

/*============================================================================
   This is the generic channel interface for Abyss.  It includes both
   the generic interface to a channel from above and the interface
   between generic channel code and a particular channel
   implementation (e.g. POSIX socket) below.

   Abyss uses a channel to converse with an HTTP client.  A channel is
   oblivious to HTTP -- it just transports a byte stream in each direction.
============================================================================*/

#include "bool.h"
#include "int.h"
#include "xmlrpc-c/abyss.h"

struct TChannelVtbl;

void
ChannelCreate(const struct TChannelVtbl * const vtblP,
              void *                      const implP,
              TChannel **                 const channelPP);

typedef void ChannelDestroyImpl(TChannel * const channelP);

typedef void ChannelWriteImpl(TChannel *            const channelP,
                              const unsigned char * const buffer,
                              uint32_t              const len,
                              bool *                const failedP);

typedef void ChannelReadImpl(TChannel *      const channelP,
                             unsigned char * const buffer,
                             uint32_t        const len,
                             uint32_t *      const bytesReceivedP,
                             bool *          const failedP);

typedef uint32_t ChannelErrorImpl(TChannel * const channelP);

typedef void ChannelWaitImpl(TChannel * const channelP,
                             bool       const waitForRead,
                             bool       const waitForWrite,
                             uint32_t   const timems,
                             bool *     const readyToReadP,
                             bool *     const readyToWriteP,
                             bool *     const failedP);

typedef void ChannelInterruptImpl(TChannel * const channelP);

typedef void ChannelFormatPeerInfoImpl(TChannel *    const channelP,
                                       const char ** const peerStringP);

struct TChannelVtbl {
    ChannelDestroyImpl            * destroy;
    ChannelWriteImpl              * write;
    ChannelReadImpl               * read;
    ChannelWaitImpl               * wait;
    ChannelInterruptImpl          * interrupt;
    ChannelFormatPeerInfoImpl     * formatPeerInfo;
};

struct _TChannel {
    unsigned int        signature;
        /* With both background and foreground use of sockets, and
           background being both fork and pthread, it is very easy to
           screw up socket lifetime and try to destroy twice.  We use
           this signature to help catch such bugs.
        */
    void *              implP;
    struct TChannelVtbl vtbl;
};

#define TIME_INFINITE   0xffffffff

extern bool ChannelTraceIsActive;

void
ChannelInit(const char ** const errorP);

void
ChannelTerm(void);

void
ChannelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             bool *                const failedP);

void
ChannelRead(TChannel *      const channelP, 
            unsigned char * const buffer, 
            uint32_t        const len,
            uint32_t *      const bytesReceivedP,
            bool *          const failedP);

void
ChannelWait(TChannel * const channelP,
            bool       const waitForRead,
            bool       const waitForWrite,
            uint32_t   const timems,
            bool *     const readyToReadP,
            bool *     const readyToWriteP,
            bool *     const failedP);

void
ChannelInterrupt(TChannel * const channelP);

void
ChannelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP);

#endif
