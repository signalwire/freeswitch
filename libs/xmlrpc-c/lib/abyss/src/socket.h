#ifndef SOCKET_H_INCLUDED
#define SOCKET_H_INCLUDED

/*============================================================================
   This is for backward compatibility.  Abyss used to have a socket
   concept modelled after POSIX sockets, in which a single class (TSocket)
   contained two very different kinds of objects:  some analogous to
   a TChanSwitch and analogout to a TChannel.

   Now that we have TChanSwitch and TChannel, users should use those,
   but there may be old programs that use TSocket, and we want them to
   continue working.

   Actually, this may not be necessary.  There was only one release
   (1.06) that had the TSocket interface, and that release didn't
   provide any incentive to upgrade an older program to use TSocket,
   so there may be few or no users of TSocket.
============================================================================*/

#include "int.h"

#include "xmlrpc-c/abyss.h"

struct _TSocket {
    unsigned int   signature;
        /* With both background and foreground use of sockets, and
           background being both fork and pthread, it is very easy to
           screw up socket lifetime and try to destroy twice.  We use
           this signature to help catch such bugs.
        */

    /* Exactly one of 'chanSwitchP' and 'channelP' is non-null.
       That's how you know which of the two varieties of socket this is.
    */
    TChanSwitch *  chanSwitchP;
    TChannel * channelP;

    void * channelInfoP;  /* Defined only for a channel socket */
};

void
SocketCreateChannel(TChannel * const channelP,
                    void *     const channelInfoP,
                    TSocket ** const socketPP);

void
SocketCreateChanSwitch(TChanSwitch * const chanSwitchP,
                       TSocket **    const socketPP);

TChanSwitch *
SocketGetChanSwitch(TSocket * const socketP);

TChannel *
SocketGetChannel(TSocket * const socketP);

void *
SocketGetChannelInfo(TSocket * const socketP);

#endif

