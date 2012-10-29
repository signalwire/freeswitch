/*============================================================================
  socket.c
==============================================================================
  Implementation of TChannel class: A generic channel over which one can
  transport a bidirectional stream of bytes.

  A TChannel is a lot like a POSIX stream socket in "connected" state.
============================================================================*/

#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "bool.h"
#include "int.h"
#include "mallocvar.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/abyss.h"
#ifdef WIN32
  #include "socket_win.h"
#else
  #include "socket_unix.h"
#endif
#include "channel.h"



static void
socketOsInit(const char ** const errorP) {

#ifdef WIN32
    SocketWinInit(errorP);
#else
    SocketUnixInit(errorP);
#endif
}



static void
socketOsTerm(void) {

#ifdef WIN32
    SocketWinTerm();
#else
    SocketUnixTerm();
#endif
}
    


bool ChannelTraceIsActive;

void
ChannelInit(const char ** const errorP) {

    socketOsInit(errorP);

    if (!*errorP) {
        ChannelTraceIsActive = (getenv("ABYSS_TRACE_CHANNEL") != NULL);
        if (ChannelTraceIsActive)
            fprintf(stderr, "Abyss channel layer will trace channel traffic "
                    "due to ABYSS_TRACE_CHANNEL environment variable\n");
    }
}



void
ChannelTerm(void) {

    socketOsTerm();
}



/* ChannelCreate() is not exported to the Abyss user.  It is meant to
   be used by an implementation-specific TChannel generator which is
   exported to the Abyss user, e.g. ChannelCreateUnix() in
   socket_unix.c

   The TChannel generator functions are the _only_ user-accessible
   functions that are particular to an implementation.
*/

static unsigned int const channelSignature = 0x06060B;

void
ChannelCreate(const struct TChannelVtbl * const vtblP,
              void *                      const implP,
              TChannel **                 const channelPP) {

    TChannel * channelP;

    MALLOCVAR(channelP);

    if (channelP) {
        channelP->implP = implP;
        channelP->vtbl = *vtblP;
        channelP->signature = channelSignature;
        *channelPP = channelP;

        if (ChannelTraceIsActive)
            fprintf(stderr, "Created channel %p\n", channelP);
    }
}



void
ChannelDestroy(TChannel * const channelP) {

    if (ChannelTraceIsActive)
        fprintf(stderr, "Destroying channel %p\n", channelP);

    assert(channelP->signature == channelSignature);

    channelP->vtbl.destroy(channelP);

    channelP->signature = 0;  /* For debuggability */

    free(channelP);
}



void
ChannelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             bool *                const failedP) {

    if (ChannelTraceIsActive)
        fprintf(stderr, "Writing %u bytes to channel %p\n", len, channelP);

    (*channelP->vtbl.write)(channelP, buffer, len, failedP);
}



void
ChannelRead(TChannel *      const channelP, 
            unsigned char * const buffer, 
            uint32_t        const len,
            uint32_t *      const bytesReceivedP,
            bool *          const failedP) {
    
    if (ChannelTraceIsActive)
        fprintf(stderr, "Reading %u bytes from channel %p\n", len, channelP);

    (*channelP->vtbl.read)(channelP, buffer, len, bytesReceivedP, failedP);
}



void
ChannelWait(TChannel * const channelP,
            bool       const waitForRead,
            bool       const waitForWrite,
            uint32_t   const timems,
            bool *     const readyToReadP,
            bool *     const readyToWriteP,
            bool *     const failedP) {

    if (ChannelTraceIsActive) {
        if (waitForRead)
            fprintf(stderr, "Waiting %u milliseconds for data from "
                    "channel %p\n", timems, channelP);
        if (waitForWrite)
            fprintf(stderr, "Waiting %u milliseconds for channel %p "
                    "to be writable\n", timems, channelP);
    }
    (*channelP->vtbl.wait)(channelP, waitForRead, waitForWrite, timems,
                           readyToReadP, readyToWriteP, failedP);
}



void
ChannelInterrupt(TChannel * const channelP) {

    if (ChannelTraceIsActive)
        fprintf(stderr, "Interrupting channel waits");

    (*channelP->vtbl.interrupt)(channelP);
}



void
ChannelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP) {

    (*channelP->vtbl.formatPeerInfo)(channelP, peerStringP);
}

