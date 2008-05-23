/*============================================================================
  socket.c
==============================================================================
  Implementation of obsolete TSocket class.

  Use TChannel and TChanSwitch instead for new work.
============================================================================*/

#include <assert.h>
#include <stdlib.h>

#include "mallocvar.h"
#include "xmlrpc-c/abyss.h"
#include "channel.h"
#include "chanswitch.h"

#include "socket.h"


/* SocketCreate...() is not exported to the Abyss user.  It is meant to
   be used by an implementation-specific TSocket generator which is
   exported to the Abyss user, e.g. SocketCreateUnix() in
   socket_unix.c

   The TSocket generator functions are the _only_ user-accessible
   functions that are particular to an implementation.
*/

static uint const socketSignature = 0x060609;


static void
socketCreate(TSocket ** const socketPP) {

    TSocket * socketP;

    MALLOCVAR(socketP);

    if (socketP) {
        socketP->signature   = socketSignature;
        *socketPP = socketP;
    } else
        *socketPP = NULL;
}



void
SocketCreateChannel(TChannel * const channelP,
                    void *     const channelInfoP,
                    TSocket ** const socketPP) {

    TSocket * socketP;

    socketCreate(&socketP);

    if (socketP) {
        socketP->channelP     = channelP;
        socketP->chanSwitchP  = NULL;
        socketP->channelInfoP = channelInfoP;
        *socketPP = socketP;
    } else
        *socketPP = NULL;
}



void
SocketCreateChanSwitch(TChanSwitch * const chanSwitchP,
                       TSocket **    const socketPP) {

    TSocket * socketP;

    socketCreate(&socketP);

    if (socketP) {
        socketP->channelP    = NULL;
        socketP->chanSwitchP = chanSwitchP;
        *socketPP = socketP;
    } else
        *socketPP = NULL;
}



void
SocketDestroy(TSocket * const socketP) {

    assert(socketP->signature == socketSignature);

    if (socketP->channelP) {
        ChannelDestroy(socketP->channelP);
        free(socketP->channelInfoP);
    }

    if (socketP->chanSwitchP)
        ChanSwitchDestroy(socketP->chanSwitchP);

    socketP->signature = 0;  /* For debuggability */

    free(socketP);
}



TChanSwitch *
SocketGetChanSwitch(TSocket * const socketP) {

    return socketP->chanSwitchP;
}



TChannel *
SocketGetChannel(TSocket * const socketP) {

    return socketP->channelP;
}



void *
SocketGetChannelInfo(TSocket * const socketP) {

    return socketP->channelInfoP;
}
