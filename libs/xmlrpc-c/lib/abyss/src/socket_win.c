/*=============================================================================
                                 socket_win.c
===============================================================================
  This is the implementation of TChanSwitch and TChannel
  for a Winsock socket.
=============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <winsock.h>
#include <errno.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "mallocvar.h"
#include "trace.h"
#include "chanswitch.h"
#include "channel.h"
#include "socket.h"
#include "xmlrpc-c/abyss.h"

#include "socket_win.h"

#ifndef socklen_t
typedef int socklen_t;
#endif

/* =============================================================
   Provided nice error strings, NOT available in system errors.
   ============================================================= */

typedef struct tagSOCKERRS {
   int err;       // WSAGetLastError() value
   char * desc;   // description of error
} SOCKERR;

/* could/should perhaps be by the actual call,
   but for now, just one big list, with some repeats
*/

SOCKERR sSockErr[] = {
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "WSAStartup must be called before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEACCES,
     "WSAEACCES - "
     "Attempt to connect datagram socket to broadcast address failed "
     "because setsockopt option SO_BROADCAST is not enabled." },
   { WSAEADDRINUSE,
     "WSAEADDRINUSE - "
     "A process on the computer is already bound to the same fully-qualified "
     "address and the socket has not been marked to allow address reuse with "
     "SO_REUSEADDR. For example, the IP address and port are bound in the "
     "af_inet case). (See the SO_REUSEADDR socket option under setsockopt.)" },
   { WSAEADDRNOTAVAIL,
     "WSAEADDRNOTAVAIL - "
     "The specified address is not a valid address for this computer." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The name or namelen parameter is not a valid part of the user "
     "address space, the namelen parameter is too small, the name parameter "
     "contains an incorrect address format for the associated "
     "address family, or the first two bytes of the memory block "
     "specified by name does not match the address family associated with "
     "the socket descriptor s." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, or the "
     "service provider is still processing a callback function." },
   { WSAEINVAL,
     "WSAEINVAL - "
     "The socket is already bound to an address." },
   { WSAENOBUFS,
     "WSAENOBUFS - "
     "Not enough buffers available, too many connections." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },

   // setsocketopt
   { WSAENETRESET,
     "WSAENETRESET - "
     "Connection has timed out when SO_KEEPALIVE is set." },
   { WSAENOPROTOOPT,
     "WSAENOPROTOOPT - "
     "The option is unknown or the specified provider "
     "or socket is not capable of implementing it "
     "(see SO_GROUP_PRIORITY limitations)." },
   { WSAENOTCONN,
     "WSAENOTCONN - "
     "Connection has been reset when SO_KEEPALIVE is set." },

   // WSAStartup
   { WSASYSNOTREADY,
     "WSASYSNOTREADY - "
     "The underlying network subsystem is not ready for "
     "network communication." },
   { WSAVERNOTSUPPORTED,
     "WSAVERNOTSUPPORTED - "
     "The version of Windows Sockets function requested is not provided "
     "by this particular Windows Sockets implementation." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 operation is in progress." },
   { WSAEPROCLIM,
     "WSAEPROCLIM - "
     "Limit on the number of tasks allowed by the Windows Sockets "
     "implementation has been reached." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The lpWSAData is not a valid pointer." },
   // listen
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "A successful WSAStartup call must occur before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEADDRINUSE,
     "WSAEADDRINUSE - "
     "The socket's local address is already in use and the socket "
     "was not marked to allow address reuse with SO_REUSEADDR.  "
     "This error usually occurs during execution of the bind function, "
     "but could be delayed until this function if the bind was to "
     "a partially wildcard address (involving ADDR_ANY) "
     "and if a specific address needs to be committed at the time "
     "of this function call." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, "
     "or the service provider is still processing a callback function." },
   { WSAEINVAL,
     "WSAEINVAL - "
     "The socket has not been bound with bind." },
   { WSAEISCONN,
     "WSAEISCONN - "
     "The socket is already connected." },
   { WSAEMFILE,
     "WSAEMFILE - "
     "No more socket descriptors are available." },
   { WSAENOBUFS,
     "WSAENOBUFS - "
     "No buffer space is available." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },
   { WSAEOPNOTSUPP,
     "WSAEOPNOTSUPP - "
     "The referenced socket is not of a type that has a listen operation." },

   // getpeername
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "A successful WSAStartup call must occur before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The name or the namelen parameter is not a valid part of the "
     "user address space, or the namelen parameter is too small." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, "
     "or the service provider is still processing a callback function." },
   { WSAENOTCONN,
     "WSAENOTCONN - "
     "The socket is not connected." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },

   // accept
   { WSANOTINITIALISED,
     "WSANOTINITIALISED - "
     "A successful WSAStartup call must occur before using this function." },
   { WSAENETDOWN,
     "WSAENETDOWN - "
     "The network subsystem has failed." },
   { WSAEFAULT,
     "WSAEFAULT - "
     "The addrlen parameter is too small or addr is not a valid part "
     "of the user address space." },
   { WSAEINTR,
     "WSAEINTR - "
     "A blocking Windows Sockets 1.1 call was canceled through "
     "WSACancelBlockingCall." },
   { WSAEINPROGRESS,
     "WSAEINPROGRESS - "
     "A blocking Windows Sockets 1.1 call is in progress, "
     "or the service provider is still processing a callback function." },
   { WSAEINVAL,
     "WSAEINVAL - "
     "The listen function was not invoked prior to accept." },
   { WSAEMFILE,
     "WSAEMFILE - "
     "The queue is nonempty upon entry to accept and "
     "there are no descriptors available." },
   { WSAENOBUFS,
     "WSAENOBUFS - "
     "No buffer space is available." },
   { WSAENOTSOCK,
     "WSAENOTSOCK - "
     "The descriptor is not a socket." },
   { WSAEOPNOTSUPP,
     "WSAEOPNOTSUPP - "
     "The referenced socket is not a type that offers connection-oriented "
     "service." },
   { WSAEWOULDBLOCK,
     "WSAEWOULDBLOCK - "
     "The socket is marked as nonblocking and no connections are present "
     "to be accepted." },

   /* must be last entry */
   { 0,            0 }
};



static const char *
getWSAError(int const wsaErrno) {

    SOCKERR * pseP;
  
    pseP = &sSockErr[0];  // initial value
   
    while (pseP->desc) {
        if (pseP->err == wsaErrno)
            return pseP->desc;
        
        ++pseP;
    }

    return "(no description available)";
}



struct socketWin {
/*----------------------------------------------------------------------------
   The properties/state of a TSocket unique to a Unix TSocket.
-----------------------------------------------------------------------------*/
    SOCKET winsock;
    bool userSuppliedWinsock;
        /* 'socket' was supplied by the user; it belongs to him */
};

static
bool
connected(SOCKET const fd) {
/*----------------------------------------------------------------------------
   Return TRUE iff the socket on file descriptor 'fd' is in the connected
   state.
   If 'fd' does not identify a stream socket or we are unable to determine
   the state of the stream socket, the answer is "false".
-----------------------------------------------------------------------------*/
    bool connected;
    struct sockaddr sockaddr;
    socklen_t nameLen;
    int rc;

    nameLen = sizeof(sockaddr);

    rc = getpeername(fd, &sockaddr, &nameLen);

    if (rc == 0)
        connected = TRUE;
    else
        connected = FALSE;

    return connected;
}



void
SocketWinInit(const char ** const errorP) {

    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
 
    wVersionRequested = MAKEWORD(1, 0);
 
    err = WSAStartup(wVersionRequested, &wsaData);

    if (err != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "WSAStartup() faild with error %d (%s)",
                        lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



void
SocketWinTerm(void) {
    
    WSACleanup();
}



/*=============================================================================
      TChannel
=============================================================================*/

static ChannelDestroyImpl channelDestroy;

static void
channelDestroy(TChannel * const channelP) {

    struct socketWin * const socketWinP = channelP->implP;

    if (!socketWinP->userSuppliedWinsock)
        closesocket(socketWinP->winsock);

    free(socketWinP);
}



static ChannelWriteImpl channelWrite;

static void
channelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             bool *          const failedP) {

    struct socketWin * const socketWinP = channelP->implP;

    size_t bytesLeft;
    bool error;

    assert(sizeof(size_t) >= sizeof(len));

    for (bytesLeft = len, error = FALSE;
         bytesLeft > 0 && !error;
        ) {
        size_t const maxSend = (size_t)(-1) >> 1;

        int rc;
        
        rc = send(socketWinP->winsock, &buffer[len-bytesLeft],
                  MIN(maxSend, bytesLeft), 0);

        if (rc <= 0)
            /* 0 means connection closed; < 0 means severe error */
            error = TRUE;
        else
            bytesLeft -= rc;
    }
    *failedP = error;
}



static ChannelReadImpl channelRead;

static void
channelRead(TChannel *   const channelP, 
            unsigned char * const buffer, 
            uint32_t     const bufferSize,
            uint32_t *   const bytesReceivedP,
            bool * const failedP) {

    struct socketWin * const socketWinP = channelP->implP;

    int rc;
    rc = recv(socketWinP->winsock, buffer, bufferSize, 0);

    if (rc < 0) {
        *failedP = TRUE;
    } else {
        *failedP = FALSE;
        *bytesReceivedP = rc;
    }
}



static ChannelWaitImpl channelWait;

static void
channelWait(TChannel * const channelP,
            bool       const waitForRead,
            bool       const waitForWrite,
            uint32_t   const timems,
            bool *     const readyToReadP,
            bool *     const readyToWriteP,
            bool *     const failedP) {

    struct socketWin * const socketWinP = channelP->implP;

    fd_set rfds, wfds;
    TIMEVAL tv;
    bool failed, readRdy, writeRdy, timedOut;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    if (waitForRead)
        FD_SET(socketWinP->winsock, &rfds);

    if (waitForWrite)
        FD_SET(socketWinP->winsock, &wfds);

    tv.tv_sec  = timems / 1000;
    tv.tv_usec = timems % 1000;
 
    for (failed = FALSE, readRdy = FALSE, writeRdy = FALSE, timedOut = FALSE;
         !failed && !readRdy && !writeRdy && !timedOut;
        ) {

        int rc;

        rc = select(socketWinP->winsock + 1, &rfds, &wfds, NULL,
                    (timems == TIME_INFINITE ? NULL : &tv));

        switch(rc) {   
        case 0:
            timedOut = TRUE;
            break;
        case -1:  /* socket error */
            if (errno != EINTR)
                failed = TRUE;
            break;
        default:
            if (FD_ISSET(socketWinP->winsock, &rfds))
                readRdy = TRUE;
            if (FD_ISSET(socketWinP->winsock, &wfds))
                writeRdy = TRUE;
        }
    }

    if (failedP)
        *failedP       = failed;
    if (readyToReadP)
        *readyToReadP  = readRdy;
    if (readyToWriteP)
        *readyToWriteP = writeRdy;
}



static ChannelInterruptImpl channelInterrupt;

static void
channelInterrupt(TChannel * const channelP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in channelWait()
  now or in the future.

  Actually, this is just a no-op because we don't yet know how to
  accomplish that.
-----------------------------------------------------------------------------*/

}



void
ChannelWinGetPeerName(TChannel *           const channelP,
                      struct sockaddr_in * const inAddrP,
                      const char **        const errorP) {

    struct socketWin * const socketWinP = channelP->implP;

    socklen_t addrlen;
    int rc;
    struct sockaddr sockAddr;

    addrlen = sizeof(sockAddr);
    
    rc = getpeername(socketWinP->winsock, &sockAddr, &addrlen);

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "getpeername() failed.  WSAERROR = %d (%s)",
                        lastError, getWSAError(lastError));
    } else {
        if (addrlen != sizeof(sockAddr))
            xmlrpc_asprintf(errorP, "getpeername() returned a socket address "
                            "of the wrong size: %u.  Expected %u",
                            addrlen, sizeof(sockAddr));
        else {
            if (sockAddr.sa_family != AF_INET)
                xmlrpc_asprintf(errorP,
                                "Socket does not use the Inet (IP) address "
                                "family.  Instead it uses family %d",
                                sockAddr.sa_family);
            else {
                *inAddrP = *(struct sockaddr_in *)&sockAddr;

                *errorP = NULL;
            }
        }
    }
}



static ChannelFormatPeerInfoImpl channelFormatPeerInfo;

static void
channelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP) {

    struct socketWin * const socketWinP = channelP->implP;

    struct sockaddr sockaddr;
    socklen_t sockaddrLen;
    int rc;

    sockaddrLen = sizeof(sockaddr);
    
    rc = getpeername(socketWinP->winsock, &sockaddr, &sockaddrLen);
    
    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(peerStringP, "?? getpeername() failed.  "
                        "WSAERROR %d (%s)",
                        lastError, getWSAError(lastError));
    } else {
        switch (sockaddr.sa_family) {
        case AF_INET: {
            struct sockaddr_in * const sockaddrInP =
                (struct sockaddr_in *) &sockaddr;
            if (sockaddrLen < sizeof(*sockaddrInP))
                xmlrpc_asprintf(peerStringP, "??? getpeername() returned "
                                "the wrong size");
            else {
                unsigned char * const ipaddr = (unsigned char *)
                    &sockaddrInP->sin_addr.s_addr;
                xmlrpc_asprintf(peerStringP, "%u.%u.%u.%u:%hu",
                                ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3],
                                sockaddrInP->sin_port);
            }
        } break;
        default:
            xmlrpc_asprintf(peerStringP, "??? AF=%u", sockaddr.sa_family);
        }
    }
}



static struct TChannelVtbl const channelVtbl = {
    &channelDestroy,
    &channelWrite,
    &channelRead,
    &channelWait,
    &channelInterrupt,
    &channelFormatPeerInfo,
};



static void
makeChannelFromWinsock(SOCKET        const winsock,
                       TChannel **   const channelPP,
                       const char ** const errorP) {

    struct socketWin * socketWinP;

    MALLOCVAR(socketWinP);
    
    if (socketWinP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Windows "
                        "socket descriptor");
    else {
        TChannel * channelP;
        
        socketWinP->winsock = winsock;
        socketWinP->userSuppliedWinsock = TRUE;
        
        ChannelCreate(&channelVtbl, socketWinP, &channelP);
        
        if (channelP == NULL)
            xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                            "channel descriptor.");
        else {
            *channelPP = channelP;
            *errorP = NULL;
        }
        if (*errorP)
            free(socketWinP);
    }
}



static void
makeChannelInfo(struct abyss_win_chaninfo ** const channelInfoPP,
                struct sockaddr              const peerAddr,
                socklen_t                    const peerAddrLen,
                const char **                const errorP) {

    struct abyss_win_chaninfo * channelInfoP;

    MALLOCVAR(channelInfoP);
    
    if (channelInfoP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory");
    else {
        channelInfoP->peerAddrLen = peerAddrLen;
        channelInfoP->peerAddr    = peerAddr;
        
        *channelInfoPP = channelInfoP;

        *errorP = NULL;
    }
}



void
ChannelWinCreateWinsock(SOCKET                       const fd,
                        TChannel **                  const channelPP,
                        struct abyss_win_chaninfo ** const channelInfoPP,
                        const char **                const errorP) {

    struct sockaddr peerAddr;
    socklen_t peerAddrLen;
    int rc;

    peerAddrLen = sizeof(peerAddrLen);

    rc = getpeername(fd, &peerAddr, &peerAddrLen);

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        if (lastError == WSAENOTCONN) {
            /* NOTE: This specific string 'not in connected' is
               required by one of the rpctest suite items, in abyss.c
               (line 186), hence the separation of the error messages
               in this case ...
            */
            xmlrpc_asprintf(errorP, "Socket on file descriptor %d "
                            "is not in connected state. WSAERROR = %d (%s)",
                            fd, lastError, getWSAError(lastError));
        } else
            xmlrpc_asprintf(errorP, "getpeername() failed. WSAERROR = %d (%s)",
                        lastError, getWSAError(lastError));
    } else {
        makeChannelInfo(channelInfoPP, peerAddr, peerAddrLen, errorP);
        if (!*errorP) {
            makeChannelFromWinsock(fd, channelPP, errorP);

            if (*errorP)
                free(*channelInfoPP);
        }
    }
}


/*=============================================================================
      TChanSwitch
=============================================================================*/

static SwitchDestroyImpl chanSwitchDestroy;

void
chanSwitchDestroy(TChanSwitch * const chanSwitchP) {

    struct socketWin * const socketWinP = chanSwitchP->implP;

    if (!socketWinP->userSuppliedWinsock)
        closesocket(socketWinP->winsock);

    free(socketWinP);
}



static SwitchListenImpl chanSwitchListen;

static void
chanSwitchListen(TChanSwitch * const chanSwitchP,
                 uint32_t      const backlog,
                 const char ** const errorP) {

    struct socketWin * const socketWinP = chanSwitchP->implP;

    int32_t const minus1 = -1;

    int rc;

    /* Disable the Nagle algorithm to make persistant connections faster */

    setsockopt(socketWinP->winsock, IPPROTO_TCP, TCP_NODELAY,
               (const char *)&minus1, sizeof(minus1));

    rc = listen(socketWinP->winsock, backlog);

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "setsockopt() failed with WSAERROR %d (%s)",
                        lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



static SwitchAcceptImpl  chanSwitchAccept;

static void
chanSwitchAccept(TChanSwitch * const chanSwitchP,
                 TChannel **   const channelPP,
                 void **       const channelInfoPP,
                 const char ** const errorP) {
/*----------------------------------------------------------------------------
   Accept a connection via the channel switch *chanSwitchP.  Return as
   *channelPP the channel for the accepted connection.

   If no connection is waiting at *chanSwitchP, wait until one is.

   If we receive a signal while waiting, return immediately with
   *channelPP == NULL.
-----------------------------------------------------------------------------*/
    struct socketWin * const listenSocketP = chanSwitchP->implP;

    bool interrupted;
    TChannel * channelP;

    interrupted = FALSE; /* Haven't been interrupted yet */
    channelP    = NULL;  /* No connection yet */
    *errorP     = NULL;  /* No error yet */

    while (!channelP && !*errorP && !interrupted) {
        struct sockaddr peerAddr;
        socklen_t size = sizeof(peerAddr);
        int rc;

        rc = accept(listenSocketP->winsock, &peerAddr, &size);

        if (rc >= 0) {
            int const acceptedWinsock = rc;
            struct socketWin * acceptedSocketP;

            MALLOCVAR(acceptedSocketP);

            if (!acceptedSocketP)
                xmlrpc_asprintf(errorP, "Unable to allocate memory");
            else {
                acceptedSocketP->winsock = acceptedWinsock;
                acceptedSocketP->userSuppliedWinsock = FALSE;
                
                *channelInfoPP = NULL;

                ChannelCreate(&channelVtbl, acceptedSocketP, &channelP);
                if (!channelP)
                    xmlrpc_asprintf(errorP,
                                    "Failed to create TChannel object.");
                else
                    *errorP = NULL;
                
                if (*errorP)
                    free(acceptedSocketP);
            }
            if (*errorP)
                closesocket(acceptedWinsock);
        } else if (errno == EINTR)
            interrupted = TRUE;
        else
            xmlrpc_asprintf(errorP, "accept() failed, errno = %d (%s)",
                            errno, strerror(errno));
    }
    *channelPP = channelP;
}



static SwitchInterruptImpl chanSwitchInterrupt;

static void
chanSwitchInterrupt(TChanSwitch * const chanSwitchP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in chanSwitchAccept()
  now or in the future.

  Actually, this is just a no-op because we don't yet know how to
  accomplish that.
-----------------------------------------------------------------------------*/
    struct socketWin * const socketWinP = chanSwitchP->implP;

    if (!socketWinP->userSuppliedWinsock)
        closesocket(socketWinP->winsock);

}



static struct TChanSwitchVtbl const chanSwitchVtbl = {
    &chanSwitchDestroy,
    &chanSwitchListen,
    &chanSwitchAccept,
    &chanSwitchInterrupt,
};



static void
setSocketOptions(SOCKET        const fd,
                 const char ** const errorP) {

    int32_t const n = 1;

    int rc;

    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n));

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "Failed to set socket options.  "
                        "setsockopt() failed with WSAERROR %d (%s)",
                        lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



void
bindSocketToPort(SOCKET           const winsock,
                 struct in_addr * const addrP,
                 uint16_t         const portNumber,
                 const char **    const errorP) {
    
    struct sockaddr_in name;
    int rc;
	int one = 1;

    ZeroMemory(&name, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    if (addrP)
        name.sin_addr = *addrP;

	setsockopt(winsock, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(int));
    rc = bind(winsock, (struct sockaddr *)&name, sizeof(name));

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "Unable to bind socket to port number %u.  "
                        "bind() failed with WSAERROR %i (%s)",
                        portNumber, lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



void
ChanSwitchWinCreate(uint16_t       const portNumber,
                    TChanSwitch ** const chanSwitchPP,
                    const char **  const errorP) {
/*----------------------------------------------------------------------------
   Create a Winsock-based channel switch.

   Set the socket's local address so that a subsequent "listen" will listen
   on all IP addresses, port number 'portNumber'.
-----------------------------------------------------------------------------*/
    struct socketWin * socketWinP;

    MALLOCVAR(socketWinP);

    if (!socketWinP)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Windows socket "
                        "descriptor structure.");
    else {
        SOCKET winsock;

        winsock = socket(AF_INET, SOCK_STREAM, 0);

        if (winsock == 0 || winsock == INVALID_SOCKET) {
            int const lastError = WSAGetLastError();
            xmlrpc_asprintf(errorP, "socket() failed with WSAERROR %d (%s)",
                            lastError, getWSAError(lastError));
        } else {
            socketWinP->winsock = winsock;
            socketWinP->userSuppliedWinsock = FALSE;
            
            setSocketOptions(socketWinP->winsock, errorP);
            if (!*errorP) {
                bindSocketToPort(socketWinP->winsock, NULL, portNumber,
                                 errorP);
                if (!*errorP)
                    ChanSwitchCreate(&chanSwitchVtbl, socketWinP,
                                     chanSwitchPP);
            }

            if (*errorP)
                closesocket(winsock);
        }
        if (*errorP)
            free(socketWinP);
    }
}



void
ChanSwitchWinCreateWinsock(SOCKET         const winsock,
                           TChanSwitch ** const chanSwitchPP,
                           const char **  const errorP) {

    struct socketWin * socketWinP;

    if (connected(winsock))
        xmlrpc_asprintf(errorP, "Socket is in connected state.");
    else {
        MALLOCVAR(socketWinP);

        if (socketWinP == NULL)
            xmlrpc_asprintf(errorP, "unable to allocate memory for Windows "
                            "socket descriptor.");
        else {
            TChanSwitch * chanSwitchP;

            socketWinP->winsock = winsock;
            socketWinP->userSuppliedWinsock = TRUE;
            
            ChanSwitchCreate(&chanSwitchVtbl, socketWinP, &chanSwitchP);

            if (chanSwitchP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel switch descriptor");
            else {
                *chanSwitchPP = chanSwitchP;
                *errorP = NULL;
            }
            if (*errorP)
                free(socketWinP);
        }
    }
}
